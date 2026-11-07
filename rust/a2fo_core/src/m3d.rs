//! Legacy M3D to Storm3D SOD 1.93 conversion. The C++ boundary owns files and
//! public STL results; this module owns parsing, transforms, and serialization.
// Keep the legacy field order, bounded indices and ABI-shaped arguments auditable.
#![allow(
    clippy::needless_range_loop,
    clippy::field_reassign_with_default,
    clippy::unnecessary_filter_map
)]
use alloc::{
    collections::{BTreeMap, BTreeSet},
    format,
    string::ToString,
    vec,
    vec::Vec,
};
use core::{ffi::c_void, ptr, slice};

type Bytes = Vec<u8>;
fn b(s: &str) -> Bytes {
    s.as_bytes().to_vec()
}
fn join(parts: &[&[u8]]) -> Bytes {
    let mut out = Vec::new();
    for p in parts {
        out.extend_from_slice(p);
    }
    out
}
fn num(n: usize) -> Bytes {
    n.to_string().into_bytes()
}
fn finite(v: f32) -> f32 {
    if v.is_finite() { v } else { 0.0 }
}
fn at_least(v: f32, t: f32) -> bool {
    v > t - 0.005
}
fn above(v: f32, t: f32) -> bool {
    v > t + 0.005
}
fn lower_ascii(s: &[u8]) -> Bytes {
    s.iter()
        .map(|c: &u8| if c.is_ascii_uppercase() { c + 32 } else { *c })
        .collect()
}
fn authored(n: &Node) -> Bytes {
    lower_ascii(if n.source_name.is_empty() {
        &n.name
    } else {
        &n.source_name
    })
}

#[derive(Default, Clone)]
struct Material {
    name: Bytes,
    ambient: [f32; 3],
    diffuse: [f32; 3],
    specular: [f32; 3],
    shininess: f32,
    flags: u32,
    flags2: u32,
}
#[derive(Default, Clone)]
struct Face {
    positions: [u16; 3],
    uvs: [u16; 3],
}
#[derive(Default, Clone)]
struct Group {
    material: Bytes,
    diffuse: Bytes,
    bump: Bytes,
    specular: Bytes,
    glow: Bytes,
    detail: Bytes,
    shader: Bytes,
    faces: Vec<Face>,
}
#[derive(Clone)]
struct Mesh {
    flags: u32,
    trailer: u16,
    vertices: Vec<[f32; 3]>,
    uvs: Vec<[f32; 2]>,
    groups: Vec<Group>,
    cull: i8,
}
impl Default for Mesh {
    fn default() -> Self {
        Self {
            flags: 0,
            trailer: 0,
            vertices: Vec::new(),
            uvs: Vec::new(),
            groups: Vec::new(),
            cull: 1,
        }
    }
}
#[derive(Clone)]
struct Node {
    kind: u16,
    source_id: u32,
    source_parent_id: u32,
    numeric: bool,
    source_name: Bytes,
    name: Bytes,
    parent: Bytes,
    matrix: [f32; 12],
    mesh: Mesh,
    payload: Bytes,
}
impl Default for Node {
    fn default() -> Self {
        Self {
            kind: 0,
            source_id: 0,
            source_parent_id: 0,
            numeric: false,
            source_name: Vec::new(),
            name: Vec::new(),
            parent: Vec::new(),
            matrix: [1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.],
            mesh: Mesh::default(),
            payload: Vec::new(),
        }
    }
}
#[derive(Default, Clone)]
struct Animation {
    node: Bytes,
    kind: u8,
    period: f32,
    matrices: Vec<[f32; 12]>,
    scalars: Vec<f32>,
    supported: bool,
}
#[derive(Default, Clone)]
struct Reference {
    kind: i8,
    node: Bytes,
    animation: Bytes,
    extra: u32,
}
#[derive(Default, Clone)]
struct File {
    version: f32,
    materials: Vec<Material>,
    nodes: Vec<Node>,
    animations: Vec<Animation>,
    references: Vec<Reference>,
}

struct Reader<'a> {
    data: &'a [u8],
    at: usize,
    error: Bytes,
}
impl<'a> Reader<'a> {
    fn new(data: &'a [u8]) -> Self {
        Self {
            data,
            at: 0,
            error: Vec::new(),
        }
    }
    fn remaining(&self) -> usize {
        self.data.len().saturating_sub(self.at)
    }
    fn raw(&mut self, n: usize, what: &str) -> Option<&'a [u8]> {
        if n > self.remaining() {
            self.error = b(&format!(
                "Unexpected end of M3D at byte {} while reading {}.",
                self.at, what
            ));
            return None;
        }
        let p = self.at;
        self.at += n;
        Some(&self.data[p..self.at])
    }
    fn skip(&mut self, n: usize, what: &str) -> Option<()> {
        self.raw(n, what).map(|_| ())
    }
    fn u8(&mut self, what: &str) -> Option<u8> {
        Some(self.raw(1, what)?[0])
    }
    fn i8(&mut self, what: &str) -> Option<i8> {
        Some(self.u8(what)? as i8)
    }
    fn u16(&mut self, what: &str) -> Option<u16> {
        Some(u16::from_le_bytes(self.raw(2, what)?.try_into().ok()?))
    }
    fn u32(&mut self, what: &str) -> Option<u32> {
        Some(u32::from_le_bytes(self.raw(4, what)?.try_into().ok()?))
    }
    fn f32(&mut self, what: &str) -> Option<f32> {
        Some(f32::from_bits(self.u32(what)?))
    }
    fn id(&mut self, what: &str) -> Option<Bytes> {
        let n = self.u16(what)? as usize;
        if n > self.remaining() {
            self.error = b(&format!(
                "Invalid M3D identifier length while reading {}.",
                what
            ));
            return None;
        }
        let mut s = self.raw(n, what)?.to_vec();
        while s.last() == Some(&0) {
            s.pop();
        }
        Some(s)
    }
    fn fail<T>(&mut self, s: &str) -> Option<T> {
        self.error = b(s);
        None
    }
    fn vec3(&mut self, what: &str) -> Option<[f32; 3]> {
        let mut a = [0.; 3];
        for v in &mut a {
            *v = finite(self.f32(what)?);
        }
        Some(a)
    }
    fn texture(&mut self, what: &str) -> Option<(Bytes, u32)> {
        let name = self.id(what)?;
        let flags = self.u32("texture flags")?;
        Some((name, flags))
    }
    fn index(&mut self, wide: bool, what: &str) -> Option<u16> {
        if !wide {
            return self.u16(what);
        }
        let v = self.u32(what)?;
        if v > u16::MAX as u32 {
            return self.fail("M3D index exceeds Armada's 16-bit SOD limit.");
        }
        Some(v as u16)
    }
    fn group(&mut self, version: f32, mesh_flags: u32) -> Option<Group> {
        let mut g = Group::default();
        let faces = self.u16("group face count")? as usize;
        g.material = self.id("group material")?;
        if at_least(version, 1.99) {
            g.diffuse = self.texture("diffuse texture")?.0;
            g.bump = self.texture("bump texture")?.0;
            g.specular = self.texture("specular texture")?.0;
            g.glow = self.texture("glow texture")?.0;
            if at_least(version, 2.0) {
                g.detail = self.texture("detail texture")?.0;
            }
        } else {
            let Some(n) = self.u32("group texture count") else {
                return self.fail("Invalid M3D group texture count.");
            };
            if n > 1024 {
                return self.fail("Invalid M3D group texture count.");
            }
            for i in 0..n {
                let (name, kind) = self.texture("group texture")?;
                match i {
                    0 => g.diffuse = name,
                    1 => g.bump = name,
                    2 => g.specular = name,
                    3 => g.glow = name,
                    4 => g.detail = name,
                    _ => (),
                }
                if kind & 0x200 != 0 {
                    self.f32("legacy bump strength")?;
                }
            }
        }
        g.shader = self.id("group shader")?;
        if at_least(version, 2.05) {
            self.id("group extra shader data")?;
        }
        g.faces = vec![Face::default(); faces];
        if !at_least(version, 1.97) {
            for f in &mut g.faces {
                for c in 0..3 {
                    f.positions[c] = self.u16("position index")?;
                    f.uvs[c] = self.u16("UV index")?;
                }
            }
            return Some(g);
        }
        let wide = at_least(version, 2.06);
        let mut corners = vec![[0u16; 3]; faces];
        for f in &mut corners {
            for v in f {
                *v = self.index(wide, "corner record index")?;
            }
        }
        let Some(n) = self.u32("corner record count") else {
            return self.fail("Invalid M3D corner record count.");
        };
        if n > 10_000_000 {
            return self.fail("Invalid M3D corner record count.");
        }
        let mut records = Vec::with_capacity(n as usize);
        for _ in 0..n {
            let pos;
            let uv;
            if at_least(version, 2.02) {
                pos = self.index(wide, "corner attribute index")?;
                self.index(wide, "corner attribute index")?;
                uv = self.index(wide, "corner attribute index")?;
            } else {
                pos = self.u16("position index")?;
                uv = self.u16("UV index")?;
            }
            records.push((pos, uv));
        }
        for (f, indices) in g.faces.iter_mut().zip(&corners) {
            for c in 0..3 {
                let i = indices[c] as usize;
                if i >= records.len() {
                    return self.fail("M3D face refers to an out-of-range corner record.");
                }
                f.positions[c] = records[i].0;
                f.uvs[c] = records[i].1;
            }
        }
        if mesh_flags & 2 != 0 {
            self.skip(n as usize * 24, "tangent data")?;
        }
        if version >= 1.9925 {
            self.skip(24, "group bounds")?;
            let nr = self.u32("bounds record count")?;
            if nr > 10_000_000 {
                return None;
            }
            self.skip(nr as usize * 8, "bounds records")?;
            let ni = self.u32("bounds index count")?;
            if ni > 10_000_000 {
                return None;
            }
            self.skip(ni as usize * 2, "bounds indices")?;
            self.u16("bounds depth")?;
        }
        Some(g)
    }
    fn mesh(&mut self, version: f32) -> Option<Mesh> {
        let mut m = Mesh::default();
        m.flags = self.u32("mesh flags")?;
        m.trailer = self.u16("mesh trailer")?;
        let nv = self.u16("vertex count")?;
        let mut nn = nv;
        if at_least(version, 2.02) {
            nn = self.u16("normal count")?;
        }
        let nu = self.u16("UV count")?;
        let ng = self.u16("group count")?;
        for _ in 0..nv {
            m.vertices.push(self.vec3("vertex")?);
        }
        for _ in 0..nn {
            self.vec3("normal")?;
        }
        for _ in 0..nu {
            let mut uv = [self.f32("UV")?, self.f32("UV")?];
            uv[0] = finite(uv[0]);
            uv[1] = finite(uv[1]);
            m.uvs.push(uv);
        }
        for _ in 0..ng {
            m.groups.push(self.group(version, m.flags)?);
        }
        m.cull = self.i8("cull flag")?;
        if !at_least(version, 2.01) {
            let n = self.u16("mesh extra count")?;
            self.skip(n as usize * 2, "mesh extra data")?;
        }
        Some(m)
    }
}

fn parse(data: &[u8], fmt: &impl Fn(u32) -> Bytes) -> Result<File, Bytes> {
    let mut r = Reader::new(data);
    let mut f = File::default();
    let h = r.raw(10, "header");
    if h.is_none() {
        return Err(r.error);
    }
    if &h.unwrap()[..8] != b"Mad3D_SW" {
        return Err(b("Not a Star Trek Legacy M3D stream."));
    }
    f.version = r.f32("version").ok_or_else(|| r.error.clone())?;
    if !at_least(f.version, 1.94) || above(f.version, 2.06) {
        return Err(join(&[
            b"Unsupported Legacy M3D version ",
            &fmt(f.version.to_bits()),
            b"; supported native layouts are 1.94 through 2.06.",
        ]));
    }
    let n = r.u16("material count").ok_or_else(|| r.error.clone())?;
    for _ in 0..n {
        let mut m = Material::default();
        m.name = r.id("material name").ok_or_else(|| r.error.clone())?;
        m.ambient = r.vec3("ambient colour").ok_or_else(|| r.error.clone())?;
        m.diffuse = r.vec3("diffuse colour").ok_or_else(|| r.error.clone())?;
        m.specular = r.vec3("specular colour").ok_or_else(|| r.error.clone())?;
        m.shininess = r.f32("shininess").ok_or_else(|| r.error.clone())?;
        if at_least(f.version, 1.99) {
            m.flags = r.u32("material flags").ok_or_else(|| r.error.clone())?;
            m.flags2 = r.u32("material flags2").ok_or_else(|| r.error.clone())?;
        } else {
            m.flags = r.u8("material flags").ok_or_else(|| r.error.clone())? as u32;
            m.flags2 = r.u8("material flags2").ok_or_else(|| r.error.clone())? as u32;
        }
        f.materials.push(m);
    }
    let n = r.u16("node count").ok_or_else(|| r.error.clone())?;
    for _ in 0..n {
        let mut node = Node::default();
        if at_least(f.version, 1.96) {
            node.numeric = true;
            node.source_name = r.id("node name").ok_or_else(|| r.error.clone())?;
            node.source_id = r.u32("node ID").ok_or_else(|| r.error.clone())?;
            node.kind = r.u16("node type").ok_or_else(|| r.error.clone())?;
            node.source_parent_id = r.u32("parent ID").ok_or_else(|| r.error.clone())?;
            node.name = node.source_name.clone();
        } else {
            node.kind = r.u16("node type").ok_or_else(|| r.error.clone())?;
            node.name = r.id("node name").ok_or_else(|| r.error.clone())?;
            node.parent = r.id("parent name").ok_or_else(|| r.error.clone())?;
            node.source_name = node.name.clone();
        }
        for v in &mut node.matrix {
            *v = r.f32("node matrix").ok_or_else(|| r.error.clone())?;
        }
        if node.matrix.iter().any(|v| !v.is_finite()) {
            node.matrix = Node::default().matrix;
        }
        match node.kind {
            1 => node.mesh = r.mesh(f.version).ok_or_else(|| r.error.clone())?,
            2 => {
                r.u8("effect subtype").ok_or_else(|| r.error.clone())?;
                r.skip(20, "effect data").ok_or_else(|| r.error.clone())?;
            }
            3 | 12 => node.payload = r.id("node command").ok_or_else(|| r.error.clone())?,
            _ => (),
        }
        f.nodes.push(node);
    }
    if at_least(f.version, 1.96) {
        let mut ids = BTreeMap::new();
        let mut names = BTreeMap::new();
        for (i, node) in f.nodes.iter().enumerate() {
            ids.insert(node.source_id, i);
            *names.entry(node.source_name.clone()).or_insert(0usize) += 1;
        }
        for node in &mut f.nodes {
            if names[&node.source_name] > 1 || node.source_name.is_empty() {
                node.name = join(&[
                    if node.source_name.is_empty() {
                        b"m3d_node"
                    } else {
                        &node.source_name
                    },
                    b"_m3d_",
                    &num(node.source_id as usize),
                ]);
            } else {
                node.name = node.source_name.clone();
            }
        }
        let has_zero = ids.contains_key(&0);
        let links: Vec<Bytes> = f
            .nodes
            .iter()
            .map(|n| {
                if n.source_parent_id == 0 && !has_zero {
                    return Vec::new();
                }
                match ids.get(&n.source_parent_id) {
                    Some(i) if f.nodes[*i].source_id != n.source_id => f.nodes[*i].name.clone(),
                    _ => Vec::new(),
                }
            })
            .collect();
        for (n, p) in f.nodes.iter_mut().zip(links) {
            n.parent = p;
        }
    }
    let n = r.u16("animation count").ok_or_else(|| r.error.clone())?;
    for _ in 0..n {
        let mut a = Animation::default();
        a.node = r.id("animation name").ok_or_else(|| r.error.clone())?;
        if at_least(f.version, 1.96) {
            r.u32("animation ID").ok_or_else(|| r.error.clone())?;
        }
        let frames = r
            .u16("animation frame count")
            .ok_or_else(|| r.error.clone())? as usize;
        a.period = f32::from_bits(r.u32("animation duration").ok_or_else(|| r.error.clone())?);
        a.kind = r.u8("animation type").ok_or_else(|| r.error.clone())?;
        let extra = r
            .u8("animation extra flag")
            .ok_or_else(|| r.error.clone())?;
        if !a.period.is_finite() || a.period < 0. {
            a.period = 0.;
        }
        if a.kind > 5 {
            a.kind = 0;
        }
        let stride = match a.kind {
            0 => 48,
            1 => 20,
            2 | 4 => 12,
            3 => 8,
            _ => 4,
        };
        let main = frames * stride;
        if main > r.remaining() {
            return Err(b("Invalid M3D animation payload length."));
        }
        if a.kind == 0 {
            for _ in 0..frames {
                let mut m = [0.; 12];
                for v in &mut m {
                    *v = r.f32("animation matrix").ok_or_else(|| r.error.clone())?;
                }
                a.matrices.push(m);
            }
        } else if a.kind == 5 {
            for _ in 0..frames {
                a.scalars
                    .push(r.f32("scalar animation").ok_or_else(|| r.error.clone())?);
            }
        } else {
            a.supported = false;
            r.skip(main, "animation payload")
                .ok_or_else(|| r.error.clone())?;
        }
        if extra != 0 {
            r.skip(frames * 4, "animation extra data")
                .ok_or_else(|| r.error.clone())?;
        }
        if a.kind == 0 || a.kind == 5 {
            a.supported = true;
        }
        f.animations.push(a);
    }
    if at_least(f.version, 1.96) {
        r.u32("database value").ok_or_else(|| r.error.clone())?;
    }
    if at_least(f.version, 1.6) {
        let n = r.u16("reference count").ok_or_else(|| r.error.clone())?;
        for _ in 0..n {
            let mut reference = Reference::default();
            reference.kind = r.u8("reference kind").ok_or_else(|| r.error.clone())? as i8;
            reference.node = r.id("reference node").ok_or_else(|| r.error.clone())?;
            reference.animation = r.id("reference animation").ok_or_else(|| r.error.clone())?;
            if at_least(f.version, 1.8) {
                reference.extra = r.u32("reference value").ok_or_else(|| r.error.clone())?;
            }
            f.references.push(reference);
        }
    }
    if r.remaining() != 0 {
        return Err(b(&format!(
            "M3D contains unrecognized trailing data at byte {}.",
            r.at
        )));
    }
    Ok(f)
}

#[derive(Default)]
struct Stats {
    source_version: f32,
    materials: usize,
    source_nodes: usize,
    output_nodes: usize,
    meshes: usize,
    groups: usize,
    triangles: usize,
    animations: usize,
    texture_references: usize,
    source_roots: usize,
    synthesized_root: bool,
    reordered_nodes: usize,
    disambiguated_names: usize,
    damage_emitters_adapted: usize,
    materials_end: usize,
    nodes_end: usize,
    animations_end: usize,
    references_end: usize,
    passthrough: bool,
    warnings: Vec<Bytes>,
    hierarchy: Vec<(Bytes, Bytes, Bytes)>,
    textures: Vec<TextureBinding>,
}
struct TextureBinding {
    mesh: Bytes,
    diffuse: Bytes,
    bump: Bytes,
    min: [f32; 3],
    max: [f32; 3],
    has_bounds: bool,
}
fn warning(s: &mut Stats, parts: &[&[u8]]) {
    s.warnings.push(join(parts));
}
fn normalize_texture(value: &[u8]) -> Bytes {
    let name = match value.iter().rposition(|c| *c == b'/' || *c == b'\\') {
        Some(i) => &value[i + 1..],
        None => value,
    };
    let lower = lower_ascii(name);
    for ext in [b".dds".as_slice(), b".tga", b".png", b".bmp"] {
        if lower.ends_with(ext) {
            return name[..name.len() - ext.len()].to_vec();
        }
    }
    name.to_vec()
}
fn sod_material(shader: &[u8]) -> &'static [u8] {
    let lower = lower_ascii(shader);
    let contains = |needle: &[u8]| lower.windows(needle.len()).any(|w| w == needle);
    if contains(b"alphathreshold") || (contains(b"alpha") && contains(b"threshold")) {
        b"alphathreshold"
    } else if contains(b"add") {
        b"additive"
    } else if contains(b"transluc") {
        b"translucent"
    } else if contains(b"wire") {
        b"wireframe"
    } else if contains(b"alpha") {
        b"alpha"
    } else {
        b"default"
    }
}
fn group_bounds(node: &Node, g: &Group) -> (bool, [f32; 3], [f32; 3]) {
    let mut min = [0.; 3];
    let mut max = [0.; 3];
    let mut have = false;
    for f in &g.faces {
        for &index in &f.positions {
            let Some(v) = node.mesh.vertices.get(index as usize) else {
                return (false, min, max);
            };
            if v.iter().any(|x| !x.is_finite()) {
                return (false, min, max);
            }
            if !have {
                min = *v;
                max = *v;
                have = true;
            } else {
                for axis in 0..3 {
                    if v[axis] < min[axis] {
                        min[axis] = v[axis];
                    }
                    if max[axis] < v[axis] {
                        max[axis] = v[axis];
                    }
                }
            }
        }
    }
    (have, min, max)
}
fn normalize_roots(source: &File, s: &mut Stats) -> File {
    let mut f = source.clone();
    let names: BTreeSet<Bytes> = f
        .nodes
        .iter()
        .filter(|n| !n.name.is_empty())
        .map(|n| n.name.clone())
        .collect();
    for node in &mut f.nodes {
        if !node.parent.is_empty() && (node.parent == node.name || !names.contains(&node.parent)) {
            warning(
                s,
                &[
                    b"Legacy M3D node '",
                    &node.name,
                    b"' references missing/self parent '",
                    &node.parent,
                    b"'; treating it as a top-level node.",
                ],
            );
            node.parent.clear();
        }
    }
    let roots: Vec<usize> = f
        .nodes
        .iter()
        .enumerate()
        .filter(|(_, n)| n.parent.is_empty())
        .map(|(i, _)| i)
        .collect();
    s.source_roots = roots.len();
    if roots.len() > 1 {
        let names: BTreeSet<Bytes> = f.nodes.iter().map(|n| n.name.clone()).collect();
        let mut root = b("a2fo_m3d_root");
        let mut suffix = 2;
        while names.contains(&root) {
            root = join(&[b"a2fo_m3d_root_", &num(suffix)]);
            suffix += 1;
        }
        for &i in &roots {
            f.nodes[i].parent = root.clone();
        }
        let n = Node {
            name: root.clone(),
            ..Node::default()
        };
        f.nodes.insert(0, n);
        s.synthesized_root = true;
        warning(
            s,
            &[
                b"Legacy M3D has ",
                &num(roots.len()),
                b" top-level nodes; inserted synthetic Storm3D root '",
                &root,
                b"' so geometry is not detached by Armada's single-root loader.",
            ],
        );
    } else if roots.is_empty() && !f.nodes.is_empty() {
        f.nodes[0].parent.clear();
        s.source_roots = 1;
        warning(
            s,
            &[
                b"Legacy M3D hierarchy contained no top-level node; promoted '",
                &f.nodes[0].name,
                b"' to the Storm3D root.",
            ],
        );
    }
    f
}

type Affine = [f64; 12];
fn compose(a: &Affine, b: &Affine) -> Affine {
    let mut out = [0.; 12];
    for col in 0..4 {
        for row in 0..3 {
            for k in 0..3 {
                out[col * 3 + row] += a[k * 3 + row] * b[col * 3 + k];
            }
            if col == 3 {
                out[col * 3 + row] += a[9 + row];
            }
        }
    }
    out
}
fn inverse(m: &Affine) -> Option<Affine> {
    let mut rows = [[0.; 6]; 3];
    for r in 0..3 {
        for c in 0..3 {
            rows[r][c] = m[c * 3 + r];
        }
        rows[r][r + 3] = 1.;
    }
    for c in 0..3 {
        let mut pivot = c;
        for r in c + 1..3 {
            if rows[r][c].abs() > rows[pivot][c].abs() {
                pivot = r;
            }
        }
        if rows[pivot][c].abs() < 1e-12 {
            return None;
        }
        rows.swap(c, pivot);
        let div = rows[c][c];
        for v in &mut rows[c] {
            *v /= div;
        }
        for r in 0..3 {
            if r == c {
                continue;
            }
            let factor = rows[r][c];
            for k in 0..6 {
                rows[r][k] -= factor * rows[c][k];
            }
        }
    }
    let mut out = [0.; 12];
    for r in 0..3 {
        for c in 0..3 {
            out[c * 3 + r] = rows[r][c + 3];
            out[9 + r] -= rows[r][c + 3] * m[9 + c];
        }
    }
    Some(out)
}
fn parent_index(node: &Node, by_name: &BTreeMap<Bytes, usize>) -> Option<usize> {
    by_name.get(&node.parent).copied()
}
fn world(i: usize, nodes: &[Node], by_name: &BTreeMap<Bytes, usize>) -> Option<Affine> {
    let mut result = [1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.];
    let mut current = Some(i);
    let mut visited = 0;
    while let Some(index) = current {
        if visited >= nodes.len() {
            return None;
        }
        visited += 1;
        let mut local = [0.; 12];
        for (dst, src) in local.iter_mut().zip(&nodes[index].matrix) {
            *dst = *src as f64;
        }
        result = compose(&local, &result);
        current = parent_index(&nodes[index], by_name);
    }
    Some(result)
}
fn adapt_damage(f: &mut File, s: &mut Stats) {
    let mut by_name = BTreeMap::new();
    let mut counts = BTreeMap::new();
    for (i, n) in f.nodes.iter().enumerate() {
        by_name.entry(n.name.clone()).or_insert(i);
        *counts.entry(authored(n)).or_insert(0usize) += 1;
    }
    for i in 0..f.nodes.len() {
        if f.nodes[i].kind != 12 {
            continue;
        }
        let Some(break_i) = parent_index(&f.nodes[i], &by_name) else {
            continue;
        };
        if authored(&f.nodes[break_i]) != b"break" {
            continue;
        }
        let Some(part_i) = parent_index(&f.nodes[break_i], &by_name) else {
            continue;
        };
        let part = authored(&f.nodes[part_i]);
        let selector: &[u8] = if part == b"l_engine" || part == b"r_engine" || part == b"engine" {
            b"engines"
        } else if part == b"weapons" {
            b"target"
        } else {
            continue;
        };
        let mut target = None;
        if counts.get(selector).copied() == Some(1) {
            for (j, n) in f.nodes.iter().enumerate() {
                if n.kind == 0
                    && authored(n) == selector
                    && let Some(parent) = parent_index(n, &by_name)
                    && authored(&f.nodes[parent]) == b"damage"
                {
                    target = Some(j);
                }
            }
        }
        let children = f.nodes.iter().any(|n| n.parent == f.nodes[i].name);
        let transform = target.and_then(|j| {
            if children || !f.animations.is_empty() || !f.references.is_empty() {
                return None;
            }
            let ew = world(i, &f.nodes, &by_name)?;
            let tw = world(j, &f.nodes, &by_name)?;
            Some((j, compose(&inverse(&tw)?, &ew)))
        });
        let Some((j, local)) = transform else {
            warning(
                s,
                &[
                    b"Damage emitter '",
                    &f.nodes[i].name,
                    b"' retained under break: requires a unique static damage/",
                    selector,
                    b" selector and invertible transform.",
                ],
            );
            continue;
        };
        if local
            .iter()
            .any(|v| !v.is_finite() || v.abs() > f32::MAX as f64)
        {
            continue;
        }
        for (dst, src) in f.nodes[i].matrix.iter_mut().zip(local) {
            *dst = src as f32;
        }
        f.nodes[i].parent = f.nodes[j].name.clone();
        s.damage_emitters_adapted += 1;
        warning(
            s,
            &[
                b"Mapped Legacy break emitter '",
                &f.nodes[i].name,
                b"' to native damage/",
                selector,
                b" with its model-space transform preserved.",
            ],
        );
    }
}
fn order_nodes(mut f: File, s: &mut Stats) -> File {
    if f.nodes.len() < 2 {
        return f;
    }
    let mut ordered = Vec::with_capacity(f.nodes.len());
    let mut emitted = vec![false; f.nodes.len()];
    let mut names = BTreeSet::new();
    let mut indices = Vec::new();
    let root = f
        .nodes
        .iter()
        .find(|n| n.parent.is_empty())
        .map(|n| n.name.clone())
        .unwrap_or_default();
    while ordered.len() < f.nodes.len() {
        let mut progress = false;
        for (i, n) in f.nodes.iter().enumerate() {
            if emitted[i] || (!n.parent.is_empty() && !names.contains(&n.parent)) {
                continue;
            }
            ordered.push(n.clone());
            indices.push(i);
            emitted[i] = true;
            if !n.name.is_empty() {
                names.insert(n.name.clone());
            }
            progress = true;
        }
        if progress {
            continue;
        }
        let Some(i) = emitted.iter().position(|e| !*e) else {
            break;
        };
        let mut n = f.nodes[i].clone();
        warning(
            s,
            &[
                b"Legacy M3D hierarchy cycle/order deadlock at node '",
                &n.name,
                b"' (parent '",
                &n.parent,
                b"'); reparented to root '",
                &root,
                b"' for Storm3D's one-pass loader.",
            ],
        );
        n.parent = root.clone();
        ordered.push(n);
        indices.push(i);
        emitted[i] = true;
        if !ordered.last().unwrap().name.is_empty() {
            names.insert(ordered.last().unwrap().name.clone());
        }
    }
    for (i, j) in indices.iter().enumerate() {
        if i != *j {
            s.reordered_nodes += 1;
        }
    }
    if s.reordered_nodes != 0 {
        warning(
            s,
            &[
                b"Reordered ",
                &num(s.reordered_nodes),
                b" Legacy node position(s) so every Storm3D parent is emitted before its children.",
            ],
        );
    }
    f.nodes = ordered;
    f
}

struct Writer {
    data: Bytes,
}
impl Writer {
    fn new() -> Self {
        Self { data: Vec::new() }
    }
    fn raw(&mut self, v: &[u8]) {
        self.data.extend_from_slice(v);
    }
    fn u8(&mut self, v: u8) {
        self.data.push(v);
    }
    fn u16(&mut self, v: u16) {
        self.raw(&v.to_le_bytes());
    }
    fn u32(&mut self, v: u32) {
        self.raw(&v.to_le_bytes());
    }
    fn f32(&mut self, v: f32) {
        self.u32(v.to_bits());
    }
    fn count(&mut self, v: usize, what: &str) -> Result<(), Bytes> {
        if v > u16::MAX as usize {
            return Err(b(&format!("{} exceeds SOD's 16-bit limit.", what)));
        }
        self.u16(v as u16);
        Ok(())
    }
    fn id(&mut self, v: &[u8], what: &str) -> Result<(), Bytes> {
        self.count(v.len(), what)?;
        self.raw(v);
        Ok(())
    }
    fn matrix(&mut self, m: &[f32; 12]) {
        for v in m {
            self.f32(finite(*v));
        }
    }
    fn size(&self) -> usize {
        self.data.len()
    }
}
fn mesh_node(w: &mut Writer, n: &Node, g: &Group) -> Result<(), Bytes> {
    if n.mesh.vertices.is_empty() {
        return Err(join(&[
            b"M3D mesh '",
            &n.name,
            b"' has no vertices; Fleet Ops Storm3D 1.93 rejects zero-vertex meshes.",
        ]));
    }
    if n.mesh.uvs.is_empty() {
        return Err(join(&[
            b"M3D mesh '",
            &n.name,
            b"' has no UV coordinates; Fleet Ops Storm3D 1.93 rejects zero-UV meshes.",
        ]));
    }
    let mut pmap = vec![u16::MAX; n.mesh.vertices.len()];
    let mut umap = vec![u16::MAX; n.mesh.uvs.len()];
    let mut vertices = Vec::new();
    let mut uvs = Vec::new();
    let mut faces = Vec::new();
    for source in &g.faces {
        let mut face = Face::default();
        for c in 0..3 {
            let pi = source.positions[c] as usize;
            let ui = source.uvs[c] as usize;
            if pi >= n.mesh.vertices.len() || ui >= n.mesh.uvs.len() {
                return Err(b("M3D mesh contains an out-of-range position or UV index."));
            }
            if pmap[pi] == u16::MAX {
                if vertices.len() >= u16::MAX as usize {
                    return Err(b(
                        "M3D material group exceeds Storm3D's 16-bit vertex limit after compaction.",
                    ));
                }
                pmap[pi] = vertices.len() as u16;
                vertices.push(n.mesh.vertices[pi]);
            }
            if umap[ui] == u16::MAX {
                if uvs.len() >= u16::MAX as usize {
                    return Err(b(
                        "M3D material group exceeds Storm3D's 16-bit UV limit after compaction.",
                    ));
                }
                umap[ui] = uvs.len() as u16;
                uvs.push(n.mesh.uvs[ui]);
            }
            face.positions[c] = pmap[pi];
            face.uvs[c] = umap[ui];
        }
        faces.push(face);
    }
    if faces.is_empty() || vertices.is_empty() || uvs.is_empty() {
        return Err(join(&[
            b"M3D mesh '",
            &n.name,
            b"' contains an empty material group; no Storm3D renderer can be built.",
        ]));
    }
    let textures = if g.diffuse.is_empty() { 0 } else { 1 };
    w.id(sod_material(&g.shader), "SOD texture material")?;
    w.u32(0);
    w.u32(textures);
    let diffuse = normalize_texture(&g.diffuse);
    if !diffuse.is_empty() {
        w.id(&diffuse, "SOD diffuse texture")?;
        w.u32(0);
    }
    w.id(b"", "SOD 1.92+ Borg texture")?;
    w.u16(0);
    w.count(vertices.len(), "SOD vertex count")?;
    w.count(uvs.len(), "SOD UV count")?;
    w.u16(1);
    for v in &vertices {
        for x in v {
            w.f32(finite(*x));
        }
    }
    for uv in &uvs {
        for x in uv {
            w.f32(finite(*x));
        }
    }
    w.count(faces.len(), "SOD face count")?;
    w.id(&g.material, "SOD lighting material")?;
    for face in &faces {
        for c in [0, 2, 1] {
            w.u16(face.positions[c]);
            w.u16(face.uvs[c]);
        }
    }
    w.u8(if n.mesh.cull == 0 { 0 } else { 1 });
    w.u16(0);
    Ok(())
}
fn serialize(source: &File, s: &mut Stats) -> Result<Bytes, Bytes> {
    for n in &source.nodes {
        if !n.source_name.is_empty() && n.name != n.source_name {
            s.disambiguated_names += 1;
        }
    }
    if s.disambiguated_names != 0 {
        warning(s,&[b"Disambiguated ",&num(s.disambiguated_names),b" duplicate Legacy node name(s) with their numeric M3D IDs so Storm3D parent links remain exact."]);
    }
    let mut f = normalize_roots(source, s);
    adapt_damage(&mut f, s);
    f = order_nodes(f, s);
    let mut w = Writer::new();
    w.raw(b"Storm3D_SW");
    w.f32(1.93);
    w.count(f.materials.len(), "SOD material count")?;
    for m in &f.materials {
        let lighting = if m.flags <= 2 { m.flags as u8 } else { 1 };
        let self_illum = if m.flags2 != 0 { 1 } else { 0 };
        w.id(&m.name, "SOD material name")?;
        for v in m.ambient {
            w.f32(v);
        }
        for v in m.diffuse {
            w.f32(v);
        }
        for v in m.specular {
            w.f32(v);
        }
        w.f32(finite(m.shininess));
        w.u8(lighting);
        w.u8(self_illum);
    }
    s.materials_end = w.size();
    let expanded: usize = f
        .nodes
        .iter()
        .map(|n| {
            if n.kind == 1 {
                n.mesh.groups.len().max(1)
            } else {
                1
            }
        })
        .sum();
    w.count(expanded, "SOD node count")?;
    for n in &f.nodes {
        if n.kind != 1 {
            let mut kind = n.kind;
            if kind == 2 {
                kind = 0;
                warning(s,&[b"Legacy effect node '",&n.name,b"' was retained as a null transform because Armada SOD 1.93 has no matching type-2 payload."]);
            } else if kind == 11 {
                kind = 0;
                warning(s,&[b"Legacy LOD node '",&n.name,b"' was neutralized to a null transform so Fleet Ops does not apply incompatible distance-based LOD selection."]);
            } else if kind != 0 && kind != 3 && kind != 12 {
                kind = 0;
                warning(
                    s,
                    &[
                        b"Unknown Legacy node type on '",
                        &n.name,
                        b"' was retained as a null transform.",
                    ],
                );
            }
            w.u16(kind);
            w.id(&n.name, "SOD node name")?;
            w.id(&n.parent, "SOD parent name")?;
            w.matrix(&n.matrix);
            if kind == 12 {
                w.id(
                    if n.payload.is_empty() {
                        &n.name
                    } else {
                        &n.payload
                    },
                    "SOD emitter",
                )?;
            }
            s.hierarchy.push((
                n.name.clone(),
                n.parent.clone(),
                if n.source_name.is_empty() {
                    n.name.clone()
                } else {
                    n.source_name.clone()
                },
            ));
            continue;
        }
        let empty = Group::default();
        let count = n.mesh.groups.len().max(1);
        for gi in 0..count {
            let g = n.mesh.groups.get(gi).unwrap_or(&empty);
            let name = if gi == 0 {
                n.name.clone()
            } else {
                join(&[&n.name, b"_a2fo_g", &num(gi + 1)])
            };
            let (has, min, max) = group_bounds(n, g);
            s.textures.push(TextureBinding {
                mesh: name.clone(),
                diffuse: g.diffuse.clone(),
                bump: g.bump.clone(),
                min,
                max,
                has_bounds: has,
            });
            w.u16(1);
            w.id(&name, "SOD mesh node name")?;
            w.id(&n.parent, "SOD parent name")?;
            w.matrix(&n.matrix);
            mesh_node(&mut w, n, g)?;
            s.hierarchy.push((
                name.clone(),
                n.parent.clone(),
                if gi == 0 && !n.source_name.is_empty() {
                    n.source_name.clone()
                } else {
                    name.clone()
                },
            ));
            let nd = normalize_texture(&g.diffuse);
            let nb = normalize_texture(&g.bump);
            if !g.diffuse.is_empty() && nd != g.diffuse {
                warning(
                    s,
                    &[
                        b"Mesh '",
                        &name,
                        b"' normalized Legacy diffuse texture '",
                        &g.diffuse,
                        b"' to Storm3D key '",
                        &nd,
                        b"'.",
                    ],
                );
            }
            if !g.bump.is_empty() && nb != g.bump {
                warning(
                    s,
                    &[
                        b"Mesh '",
                        &name,
                        b"' normalized Legacy bump texture '",
                        &g.bump,
                        b"' to Storm3D key '",
                        &nb,
                        b"'.",
                    ],
                );
            }
            if !g.bump.is_empty() && g.diffuse.is_empty() {
                warning(s,&[b"Mesh '",&n.name,b"' group ",&num(gi+1),b" has a Legacy bump texture but no diffuse texture; the bump slot cannot be represented by Fleet Ops' native DOT3 pair and was skipped."]);
            }
            if !g.specular.is_empty() || !g.glow.is_empty() || !g.detail.is_empty() {
                warning(s,&[b"Mesh '",&n.name,b"' group ",&num(gi+1),b" uses Legacy specular/glow/detail texture slots; diffuse and bump textures were preserved, remaining slots require renderer-side mapping."]);
            }
        }
    }
    s.nodes_end = w.size();
    let supported: Vec<&Animation> = f
        .animations
        .iter()
        .filter_map(|a| {
            if a.supported && (a.kind == 0 || a.kind == 5) {
                Some(a)
            } else {
                warning(
                    s,
                    &[
                        b"Animation channel '",
                        &a.node,
                        b"' uses a Legacy-only channel type and was skipped.",
                    ],
                );
                None
            }
        })
        .collect();
    w.count(supported.len(), "SOD animation channel count")?;
    for a in &supported {
        let frames = if a.kind == 5 {
            a.scalars.len()
        } else {
            a.matrices.len()
        };
        let mut period = a.period;
        if !period.is_finite() || period <= 0. {
            period = if frames > 0 { frames as f32 / 30. } else { 0. };
        }
        w.id(&a.node, "SOD animation node")?;
        w.count(frames, "SOD animation frame count")?;
        w.f32(period);
        w.u16(if a.kind == 5 { 5 } else { 0 });
        if a.kind == 5 {
            for v in &a.scalars {
                w.f32(finite(*v));
            }
        } else {
            for m in &a.matrices {
                w.matrix(m);
            }
        }
    }
    s.animations_end = w.size();
    let references: Vec<&Reference> = f
        .references
        .iter()
        .filter_map(|r| {
            if r.kind == 4 {
                Some(r)
            } else {
                warning(
                    s,
                    &[
                        b"Legacy reference for node '",
                        &r.node,
                        b"' is not a SOD texture-animation reference and was skipped.",
                    ],
                );
                None
            }
        })
        .collect();
    w.count(references.len(), "SOD texture animation reference count")?;
    for r in &references {
        w.u8(4);
        w.id(&r.node, "SOD reference node")?;
        w.id(&r.animation, "SOD reference animation")?;
        w.f32(finite(f32::from_bits(r.extra)));
    }
    s.references_end = w.size();
    s.output_nodes = expanded;
    s.animations = supported.len();
    s.texture_references = references.len();
    Ok(w.data)
}

#[repr(C)]
pub struct Event {
    kind: u32,
    first: *const u8,
    first_length: usize,
    second: *const u8,
    second_length: usize,
    third: *const u8,
    third_length: usize,
    values: [usize; 18],
    bounds_min: [f32; 3],
    bounds_max: [f32; 3],
}
impl Event {
    fn new(kind: u32) -> Self {
        Self {
            kind,
            first: ptr::null(),
            first_length: 0,
            second: ptr::null(),
            second_length: 0,
            third: ptr::null(),
            third_length: 0,
            values: [0; 18],
            bounds_min: [0.; 3],
            bounds_max: [0.; 3],
        }
    }
    fn first(mut self, v: &[u8]) -> Self {
        self.first = v.as_ptr();
        self.first_length = v.len();
        self
    }
    fn second(mut self, v: &[u8]) -> Self {
        self.second = v.as_ptr();
        self.second_length = v.len();
        self
    }
    fn third(mut self, v: &[u8]) -> Self {
        self.third = v.as_ptr();
        self.third_length = v.len();
        self
    }
}
type Emit = unsafe extern "C" fn(*mut c_void, *const Event) -> i32;
type Format = unsafe extern "C" fn(*mut c_void, u32, *mut u8, usize) -> usize;
fn send(ctx: *mut c_void, emit: Emit, event: &Event) -> bool {
    unsafe { emit(ctx, event) != 0 }
}
fn emit_stats(ctx: *mut c_void, emit: Emit, s: &Stats) -> bool {
    let mut e = Event::new(1);
    e.values = [
        s.materials,
        s.source_nodes,
        s.output_nodes,
        s.meshes,
        s.groups,
        s.triangles,
        s.animations,
        s.texture_references,
        s.source_roots,
        s.synthesized_root as usize,
        s.reordered_nodes,
        s.disambiguated_names,
        s.damage_emitters_adapted,
        s.materials_end,
        s.nodes_end,
        s.animations_end,
        s.references_end,
        s.passthrough as usize,
    ];
    e.bounds_min[0] = s.source_version;
    if !send(ctx, emit, &e) {
        return false;
    }
    for w in &s.warnings {
        if !send(ctx, emit, &Event::new(2).first(w)) {
            return false;
        }
    }
    for (name, parent, authored) in &s.hierarchy {
        if !send(
            ctx,
            emit,
            &Event::new(3).first(name).second(parent).third(authored),
        ) {
            return false;
        }
    }
    for t in &s.textures {
        let mut e = Event::new(4)
            .first(&t.mesh)
            .second(&t.diffuse)
            .third(&t.bump);
        e.values[0] = t.has_bounds as usize;
        e.bounds_min = t.min;
        e.bounds_max = t.max;
        if !send(ctx, emit, &e) {
            return false;
        }
    }
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_m3d_convert(
    data: *const u8,
    len: usize,
    ctx: *mut c_void,
    emit: Emit,
    format: Format,
) -> i32 {
    if data.is_null() && len != 0 {
        return 0;
    }
    let input = if len == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(data, len) }
    };
    let mut s = Stats::default();
    let format_version = |bits: u32| {
        let mut tmp = [0u8; 128];
        let n = unsafe { format(ctx, bits, tmp.as_mut_ptr(), tmp.len()) };
        tmp[..n.min(tmp.len())].to_vec()
    };
    let result: Result<Bytes, Bytes> =
        if input.starts_with(b"Storm3D_SW") || input.starts_with(b"StarTrekDB") {
            s.passthrough = true;
            Ok(input.to_vec())
        } else {
            parse(input, &format_version).and_then(|file| {
                s.source_version = file.version;
                s.materials = file.materials.len();
                s.source_nodes = file.nodes.len();
                for n in &file.nodes {
                    if n.kind == 1 {
                        s.meshes += 1;
                        s.groups += n.mesh.groups.len();
                        for g in &n.mesh.groups {
                            s.triangles += g.faces.len();
                        }
                    }
                }
                serialize(&file, &mut s)
            })
        };
    if !emit_stats(ctx, emit, &s) {
        return 0;
    }
    match result {
        Ok(output) => send(ctx, emit, &Event::new(5).first(&output)) as i32,
        Err(error) => {
            let _ = send(ctx, emit, &Event::new(6).first(&error));
            0
        }
    }
}
