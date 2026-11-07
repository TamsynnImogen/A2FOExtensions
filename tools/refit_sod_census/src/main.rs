//! Offline Refit replacement coverage census over an effective SOD set.
//!
//! Usage: refit_sod_census ROOT_HIGHEST [ROOT ...] > census.jsonl
//! Roots are `sod` directories in mod precedence order (child first). The first
//! root holding a case-insensitive file name wins, like FO's mod overlay.
//!
//! Static gates mirror, without changing, the current Core policy:
//! - renderer/refit/mesh_scope.cpp  mesh_model_format_supported2 (1.7..=1.93),
//!   select_opaque_mesh2 (one native group, 1..=21845 triangles, flags ⊆ two_sided)
//! - rust/a2fo_native11_model parse policy (valid_model, blend → DrawFlags).
//! Runtime gates (route 1/legacy vs MeshVB, pass 0, live pose/visibility,
//! canonical material/texture rechecks, geometry parity) are NOT evaluated here.
use a2fmt::sod::{NodeKind, Sod};
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::fmt::Write as _;
use std::path::PathBuf;

unsafe extern "C" fn accept(
    _: *mut c_void,
    _: *const a2fo_native11_model::Part,
    _: *const a2fo_native11_model::PartSource,
) -> i32 {
    1
}

fn esc(s: &str) -> String {
    let mut o = String::with_capacity(s.len() + 2);
    o.push('"');
    for c in s.chars() {
        match c {
            '"' => o.push_str("\\\""),
            '\\' => o.push_str("\\\\"),
            c if (c as u32) < 0x20 || c as u32 == 0x7f => {
                let _ = write!(o, "\\u{:04x}", c as u32);
            }
            c => o.push(c),
        }
    }
    o.push('"');
    o
}

fn header_version(b: &[u8]) -> Option<f32> {
    if b.len() < 14 || !matches!(&b[..10], b"Storm3D_SW" | b"StarTrekDB") {
        return None;
    }
    Some(f32::from_le_bytes([b[10], b[11], b[12], b[13]]))
}

fn under(sod: &Sod, node: usize, pred: impl Fn(usize) -> bool) -> bool {
    let mut at = Some(node);
    let mut guard = 0;
    while let Some(i) = at {
        if pred(i) {
            return true;
        }
        at = sod.nodes[i].parent;
        guard += 1;
        if guard > sod.nodes.len() {
            break;
        }
    }
    false
}

/// Replays each a2fo_native11_model::valid_model check separately (diagnostic only;
/// the policy crate stays authoritative). Prints every failing check.
fn why(path: &str) {
    let bytes = std::fs::read(path).expect("read");
    let sod = match Sod::parse(&bytes) {
        Ok(s) => s,
        Err(e) => return println!("{path}: a2fmt parse error {e:?}"),
    };
    let mut reasons = Vec::new();
    for node in &sod.nodes {
        if !node.transform.is_finite() || node.transform.m.iter().any(|v| v.abs() > 1_000_000.0) {
            reasons.push(format!("node {:?}: transform non-finite/out of range", node.name));
        }
        if !node.parent_name.is_empty() && node.parent.is_none() {
            reasons.push(format!("node {:?}: parent {:?} unresolved", node.name, node.parent_name));
        }
        let NodeKind::Mesh(mesh) = &node.kind else { continue };
        if mesh.cull > 1 {
            reasons.push(format!("mesh {:?}: cull={}", node.name, mesh.cull));
        }
        if mesh.positions.iter().flatten().any(|v| !v.is_finite() || v.abs() > 1_000_000.0)
            || mesh.uvs.iter().flatten().any(|v| !v.is_finite())
        {
            reasons.push(format!("mesh {:?}: non-finite position/uv", node.name));
        }
        if !mesh.groups.iter().flat_map(|g| &g.faces).flat_map(|f| f.iter()).all(|c| {
            usize::from(c.position) < mesh.positions.len() && usize::from(c.uv) < mesh.uvs.len()
        }) {
            reasons.push(format!("mesh {:?}: corner index out of range", node.name));
        }
        for g in &mesh.groups {
            if sod.lighting_material(&g.material).is_none() {
                reasons.push(format!("mesh {:?}: lighting material {:?} undefined", node.name, g.material));
            }
        }
    }
    for m in &sod.lighting_materials {
        if !m.diffuse.iter().chain(m.ambient.iter()).chain(m.specular.iter()).all(|v| v.is_finite() && *v >= 0.0)
            || !m.power.is_finite()
        {
            reasons.push(format!("material {:?}: negative/non-finite a{:?} d{:?} s{:?} p{}", m.name, m.ambient, m.diffuse, m.specular, m.power));
        }
    }
    if reasons.is_empty() {
        reasons.push("no valid_model check failed here: preflight/size/emission limits (see lib.rs)".into());
    }
    for r in reasons {
        println!("{path}: {r}");
    }
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.first().map(String::as_str) == Some("--nodes") {
        // --nodes FILE: per mesh node, the basis-row lengths of its local and
        // model transform (scale applied to normals if not re-normalised).
        let bytes = std::fs::read(&args[1]).expect("read");
        let sod = Sod::parse(&bytes).expect("parse");
        let len = |m: &[f32], r: usize| (m[r * 3] * m[r * 3] + m[r * 3 + 1] * m[r * 3 + 1] + m[r * 3 + 2] * m[r * 3 + 2]).sqrt();
        for (i, node) in sod.nodes.iter().enumerate() {
            let NodeKind::Mesh(mesh) = &node.kind else { continue };
            let local = node.transform.m;
            let model = sod.model_transform(i).m;
            println!(
                "{:24} faces={:5} local_scale={:.4}/{:.4}/{:.4} model_scale={:.4}/{:.4}/{:.4}",
                node.name, mesh.face_count(), len(&local, 0), len(&local, 1), len(&local, 2), len(&model, 0), len(&model, 1), len(&model, 2)
            );
        }
        return;
    }
    if args.first().map(String::as_str) == Some("--faces") {
        // --faces FILE NODE: per group, count faces that repeat an earlier face
        // (same position triple in any rotation / either winding), faces with a
        // repeated position index, and faces with zero model-space area.
        let bytes = std::fs::read(&args[1]).expect("read");
        let sod = Sod::parse(&bytes).expect("parse");
        let node = sod.nodes.iter().find(|n| n.name.eq_ignore_ascii_case(&args[2])).expect("node");
        let NodeKind::Mesh(mesh) = &node.kind else { panic!("not a mesh") };
        let mut all_seen: std::collections::HashMap<[u16; 3], (usize, usize)> = Default::default();
        for (gi, g) in mesh.groups.iter().enumerate() {
            let (mut dup_same, mut dup_reversed, mut dup_other_group, mut repeated, mut zero) = (0, 0, 0, 0, 0);
            let mut seen: std::collections::HashMap<[u16; 3], bool> = Default::default();
            for f in &g.faces {
                let p = [f[0].position, f[1].position, f[2].position];
                if p[0] == p[1] || p[1] == p[2] || p[0] == p[2] { repeated += 1; continue; }
                let a = mesh.positions[usize::from(p[0])];
                let b = mesh.positions[usize::from(p[1])];
                let c = mesh.positions[usize::from(p[2])];
                let u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
                let v = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
                let n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
                if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt() < 1e-9 { zero += 1; }
                // canonical rotation, keeping winding
                let r = (0..3).min_by_key(|&i| p[i]).unwrap();
                let key = [p[r], p[(r + 1) % 3], p[(r + 2) % 3]];
                let rev = [key[0], key[2], key[1]];
                if seen.contains_key(&key) { dup_same += 1 } else if seen.contains_key(&rev) { dup_reversed += 1 }
                if let Some(&(og, _)) = all_seen.get(&key) { if og != gi { dup_other_group += 1 } }
                seen.insert(key, true);
                all_seen.entry(key).or_insert((gi, 0));
            }
            println!(
                "group {gi} material={} faces={} repeated_index={repeated} zero_area={zero} duplicate_same_winding={dup_same} duplicate_reversed={dup_reversed} duplicates_earlier_group={dup_other_group}",
                g.material, g.faces.len()
            );
        }
        return;
    }
    if args.first().map(String::as_str) == Some("--materials") {
        for p in &args[1..] {
            match std::fs::read(p).map_err(|e| e.to_string()).and_then(|b| Sod::parse(&b).map_err(|e| format!("{e:?}"))) {
                Ok(s) => {
                    for m in &s.lighting_materials {
                        println!("{p}\t{}\tself_illum={}\tmodel={}\td={:?}\ta={:?}\ts={:?}\tpower={}", m.name, m.self_illuminating, m.model, m.diffuse, m.ambient, m.specular, m.power);
                    }
                }
                Err(e) => println!("{p}\terror {e}"),
            }
        }
        return;
    }
    if args.first().map(String::as_str) == Some("--why") {
        for p in &args[1..] {
            why(p);
        }
        return;
    }
    let roots: Vec<PathBuf> = args.iter().map(PathBuf::from).collect();
    if roots.is_empty() {
        eprintln!("usage: refit_sod_census ROOT_HIGHEST [ROOT ...]");
        std::process::exit(2);
    }
    // Effective set: lower-case file name → (root index, path)
    let mut effective: BTreeMap<String, (usize, PathBuf)> = BTreeMap::new();
    let mut shadowed = 0usize;
    for (ri, root) in roots.iter().enumerate() {
        let Ok(rd) = std::fs::read_dir(root) else {
            eprintln!("warning: unreadable root {}", root.display());
            continue;
        };
        let mut names: Vec<PathBuf> = rd.filter_map(|e| e.ok().map(|e| e.path())).collect();
        names.sort();
        for p in names {
            if !p.is_file() {
                continue;
            }
            let Some(n) = p.file_name().and_then(|n| n.to_str()) else { continue };
            if !n.to_ascii_lowercase().ends_with(".sod") {
                continue;
            }
            let key = n.to_ascii_lowercase();
            if effective.contains_key(&key) {
                shadowed += 1;
            } else {
                effective.insert(key, (ri, p));
            }
        }
    }
    let mut out = String::new();
    let _ = writeln!(
        out,
        "{{\"kind\":\"meta\",\"roots\":[{}],\"effective\":{},\"shadowed\":{}}}",
        roots.iter().map(|r| esc(&r.display().to_string())).collect::<Vec<_>>().join(","),
        effective.len(),
        shadowed
    );
    for (key, (ri, path)) in &effective {
        let bytes = match std::fs::read(path) {
            Ok(b) => b,
            Err(e) => {
                let _ = writeln!(out, "{{\"kind\":\"file\",\"name\":{},\"root\":{ri},\"error\":{}}}", esc(key), esc(&e.to_string()));
                continue;
            }
        };
        let version = header_version(&bytes);
        let version_ok = version.is_some_and(|v| v.is_finite() && (1.7..=1.93).contains(&v));
        let policy_ok = bytes.len() <= a2fo_native11_model::MAX_INPUT
            && a2fo_native11_model::parse_sod_detailed_all(&bytes, accept, std::ptr::null_mut());
        let parsed = Sod::parse(&bytes);
        let mut line = format!(
            "{{\"kind\":\"file\",\"name\":{},\"root\":{ri},\"bytes\":{},\"version\":{},\"version_ok\":{version_ok},\"policy_ok\":{policy_ok},\"parse_ok\":{}",
            esc(key),
            bytes.len(),
            version.map_or("null".into(), |v| if v.is_finite() { format!("{v}") } else { "null".into() }),
            parsed.is_ok()
        );
        if let Err(e) = &parsed {
            let _ = write!(line, ",\"parse_error\":{}", esc(&format!("{e:?}")));
        }
        let mut meshes = Vec::new();
        let mut node_kinds = [0usize; 5];
        if let Ok(sod) = &parsed {
            let animated: Vec<String> = sod.channels.iter().map(|c| c.node.to_ascii_lowercase()).collect();
            for (i, node) in sod.nodes.iter().enumerate() {
                let k = match &node.kind {
                    NodeKind::Null => 0,
                    NodeKind::Mesh(_) => 1,
                    NodeKind::Light(_) => 2,
                    NodeKind::Sprite => 3,
                    NodeKind::Emitter { .. } => 4,
                };
                node_kinds[k] += 1;
                let NodeKind::Mesh(mesh) = &node.kind else { continue };
                let groups: Vec<usize> = mesh.groups.iter().map(|g| g.faces.len()).collect();
                let tris: usize = groups.iter().sum();
                if tris == 0 {
                    continue; // emits nothing; not a draw candidate
                }
                let nonempty = groups.iter().filter(|&&f| f > 0).count();
                let mode = if sod.version < 1.7 {
                    format!("ident:{}", mesh.texture_material)
                } else {
                    mesh.texture_material.to_ascii_lowercase()
                };
                let mats: Vec<_> = mesh.groups.iter().filter_map(|g| sod.lighting_material(&g.material)).collect();
                let self_illum = mats.iter().filter(|m| m.self_illuminating).count();
                let specular = mats.iter().any(|m| m.specular.iter().any(|v| *v > 0.0));
                let lod = sod.lod_group(i).map(|(g, l)| {
                    let lv = sod.lod_levels(g);
                    let pos = lv.iter().position(|&x| x == l).unwrap_or(0);
                    (pos, lv.len())
                });
                let damage = under(sod, i, |j| {
                    matches!(sod.nodes[j].kind, NodeKind::Null) && sod.nodes[j].name.eq_ignore_ascii_case("damage")
                });
                let anim = under(sod, i, |j| animated.contains(&sod.nodes[j].name.to_ascii_lowercase()));
                // First static blocker, in Core gate order.
                let blocker = if !version_ok {
                    "sod_version"
                } else if !policy_ok {
                    "parser_policy"
                } else if mesh.groups.len() != 1 {
                    "multi_group"
                } else if tris > 21845 {
                    "triangle_limit"
                } else if !matches!(mode.as_str(), "opaque" | "default") {
                    "blend_mode"
                } else {
                    ""
                };
                let mut m = format!(
                    "{{\"node\":{},\"tris\":{tris},\"groups\":{},\"nonempty_groups\":{nonempty},\"mode\":{},\"cull\":{},\"bump\":{},\"height_map\":{},\"self_illum_groups\":{self_illum},\"specular\":{specular},\"damage\":{damage},\"animated\":{anim}",
                    esc(&node.name),
                    groups.len(),
                    esc(&mode),
                    mesh.cull,
                    mesh.flags & a2fmt::sod::Mesh::FLAG_BUMP != 0,
                    mesh.height_map().is_some()
                );
                match lod {
                    Some((p, n)) => { let _ = write!(m, ",\"lod\":[{p},{n}]"); }
                    None => m.push_str(",\"lod\":null"),
                }
                let gm: Vec<String> = mesh
                    .groups
                    .iter()
                    .map(|g| {
                        let si = sod.lighting_material(&g.material).map_or("null".to_string(), |x| x.self_illuminating.to_string());
                        format!("[{},{},{si}]", esc(&g.material), g.faces.len())
                    })
                    .collect();
                let _ = write!(m, ",\"group_materials\":[{}],\"blocker\":{}}}", gm.join(","), esc(blocker));
                meshes.push(m);
            }
            let _ = write!(line, ",\"channels\":{}", sod.channels.len());
        }
        let _ = write!(
            line,
            ",\"nodes\":{{\"null\":{},\"mesh\":{},\"light\":{},\"sprite\":{},\"emitter\":{}}},\"meshes\":[{}]}}",
            node_kinds[0], node_kinds[1], node_kinds[2], node_kinds[3], node_kinds[4],
            meshes.join(",")
        );
        out.push_str(&line);
        out.push('\n');
    }
    print!("{out}");
}
