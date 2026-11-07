#!/usr/bin/env python3
"""Prepare and optionally build a source-isolated A2FO menu renderer profiler.

The original renderer is read only. Each injection is pinned to one exact source
signature, so upstream changes fail preparation rather than moving a timer.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "third_party/armada2-refit/menu-renderer"
OUTPUT = ROOT / "build/menu-stage-lab/profile"
COPY = OUTPUT / "renderer"

LABELS = [
    ("HostInitialize", "host_initialize"),
    ("AssetIndex", "asset_index"),
    ("FileRead", "file_read"),
    ("OdfResolve", "odf_resolve"),
    ("SodReadParse", "sod_read_parse"),
    ("ImageDecode", "image_decode"),
    ("TextureChain", "texture_chain"),
    ("TextureUpload", "texture_upload"),
    ("FrameBegin", "frame_begin"),
    ("ShaderCreate", "shader_create"),
    ("SceneIniParse", "scene_ini_parse"),
    ("NebulaEnvelopeBake", "nebula_envelope_bake"),
    ("NebulaVolumeBake", "nebula_volume_bake"),
    ("SkyBake", "sky_bake"),
    ("MenuScene", "menu_scene"),
]

INJECTIONS = {
    "src/host.rs": [
        ('    let thread = unsafe { rt::win::GetCurrentThreadId() };\n    if THREAD.load(Ordering::Acquire) != 0 { return 0; }',
         '    let thread = unsafe { rt::win::GetCurrentThreadId() };\n    if THREAD.load(Ordering::Acquire) != 0 { return 0; }\n    crate::profile::set_owner(thread);\n    let _profile = crate::profile::Scope::new(crate::profile::Label::HostInitialize);'),
    ],
    "src/assets/mod.rs": [
        ('    fn build(dir: &str) -> DirIndex {', '    fn build(dir: &str) -> DirIndex {\n        let _profile = crate::profile::Scope::new(crate::profile::Label::AssetIndex);'),
        ('pub fn sod(name: &str) -> Option<Rc<Sod>> {', 'pub fn sod(name: &str) -> Option<Rc<Sod>> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::SodReadParse);'),
        ('pub fn odf(name: &str) -> Option<Rc<Odf>> {', 'pub fn odf(name: &str) -> Option<Rc<Odf>> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::OdfResolve);'),
        ('fn decode_image(data: &[u8]) -> Option<tga::Image> {', 'fn decode_image(data: &[u8]) -> Option<tga::Image> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::ImageDecode);'),
        ('fn load_texture(s: &mut Store, dev: Device, name: &str) -> Option<Com> {', 'fn load_texture(s: &mut Store, dev: Device, name: &str) -> Option<Com> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::TextureChain);'),
    ],
    "src/rt/mod.rs": [
        ('pub fn read_file(path: &str) -> Option<Vec<u8>> {', 'pub fn read_file(path: &str) -> Option<Vec<u8>> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::FileRead);'),
    ],
    "src/d3d/mod.rs": [
        ('pub fn upload_texture(dev: Device, w: u32, h: u32, fmt: u32, levels: &[&[u8]]) -> Option<Com> {', 'pub fn upload_texture(dev: Device, w: u32, h: u32, fmt: u32, levels: &[&[u8]]) -> Option<Com> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::TextureUpload);'),
    ],
    "src/frame/mod.rs": [
        ('fn begin(cfg: &Config) -> Option<Saved> {', 'fn begin(cfg: &Config) -> Option<Saved> {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::FrameBegin);'),
    ],
    "src/gfx/mod.rs": [
        ('        if !current(&self.obj, dev) {\n            let c = if self.pixel { dev.create_ps(self.code) } else { dev.create_vs(self.code) };',
         '        if !current(&self.obj, dev) {\n            let _profile = crate::profile::Scope::new(crate::profile::Label::ShaderCreate);\n            let c = if self.pixel { dev.create_ps(self.code) } else { dev.create_vs(self.code) };'),
    ],
    "src/menu/mod.rs": [
        # Historical label retained for log compatibility. Map::read also builds
        # stars and scene data and may resolve ODFs, so this is broader than parsing.
        ('    fn read() -> Map {', '    fn read() -> Map {\n        let _profile = crate::profile::Scope::new(crate::profile::Label::SceneIniParse);'),
        ('    let n = rt::lower(unsafe { rt::cstr_at(name, 32) });\n    draw(&n, PRect { x, y, w, h }, false) as u32',
         '    let n = rt::lower(unsafe { rt::cstr_at(name, 32) });\n    let before = crate::profile::snapshot();\n    let result = {\n        let _profile = crate::profile::Scope::new(crate::profile::Label::MenuScene);\n        draw(&n, PRect { x, y, w, h }, false) as u32\n    };\n    if result != 0 { crate::profile::report_completed(&n, before); }\n    result'),
    ],
    "src/mods/world/nebulae.rs": [
        ('fn bake_envelope(dev: Device, c: &mut Cls) -> bool {', 'fn bake_envelope(dev: Device, c: &mut Cls) -> bool {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::NebulaEnvelopeBake);'),
        ('fn bake_volume(dev: Device, c: &mut Cls, half: f32, cfg: &Settings, sh: &mut Shaders, noise: Option<&Com>) -> bool {', 'fn bake_volume(dev: Device, c: &mut Cls, half: f32, cfg: &Settings, sh: &mut Shaders, noise: Option<&Com>) -> bool {\n    let _profile = crate::profile::Scope::new(crate::profile::Label::NebulaVolumeBake);'),
    ],
    "src/mods/sky/bake.rs": [
        ('    pub fn bake(&mut self, dev: Device, r: &Recipe, edge: u32, dump: bool) -> bool {', '    pub fn bake(&mut self, dev: Device, r: &Recipe, edge: u32, dump: bool) -> bool {\n        let _profile = crate::profile::Scope::new(crate::profile::Label::SkyBake);'),
    ],
    "src/lib.rs": [
        ('pub mod menu;\n', 'pub mod menu;\n#[cfg(feature = "a2fo-menu-host")]\npub mod profile;\n'),
    ],
}


def source_files(root):
    return sorted(p for p in root.rglob("*") if p.is_file() and "target" not in p.relative_to(root).parts)


def hashes(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files(root)}


def profile_source():
    variants = "\n".join("    " + name + "," for name, _ in LABELS)
    names = ",\n".join('    "' + label + '"' for _, label in LABELS)
    return f'''//! Candidate-only, inclusive wall-clock elapsed timings on the graphics owner thread.
use alloc::string::String;
use alloc::vec::Vec;
use core::sync::atomic::{{AtomicU32, Ordering}};

#[repr(usize)]
#[derive(Clone, Copy)]
pub enum Label {{
{variants}
}}
const N: usize = {len(LABELS)};
const NAMES: [&str; N] = [
{names},
];
#[derive(Clone, Copy)]
pub struct Stat {{ calls: u64, seconds: f64 }}
const ZERO: Stat = Stat {{ calls: 0, seconds: 0.0 }};
static OWNER: AtomicU32 = AtomicU32::new(0);
static mut STATS: [Stat; N] = [ZERO; N];
static mut SEEN: Vec<String> = Vec::new();
static mut REPORTED_FIRST: bool = false;

pub fn set_owner(thread: u32) {{ OWNER.store(thread, Ordering::Release); }}
fn owner() -> bool {{
    let id = OWNER.load(Ordering::Acquire);
    id != 0 && id == unsafe {{ crate::rt::win::GetCurrentThreadId() }}
}}
pub fn snapshot() -> [Stat; N] {{
    if owner() {{ unsafe {{ STATS }} }} else {{ [ZERO; N] }}
}}
pub struct Scope {{ label: usize, start: f64, active: bool }}
impl Scope {{
    pub fn new(label: Label) -> Self {{
        let active = owner();
        Self {{ label: label as usize, start: if active {{ crate::rt::now() }} else {{ 0.0 }}, active }}
    }}
}}
impl Drop for Scope {{
    fn drop(&mut self) {{
        if self.active {{
            let elapsed = (crate::rt::now() - self.start).max(0.0);
            unsafe {{
                STATS[self.label].calls += 1;
                STATS[self.label].seconds += elapsed;
            }}
        }}
    }}
}}
fn log_stats(title: &str, before: [Stat; N], after: [Stat; N]) {{
    crate::log!("profile {{}}: inclusive wall-clock elapsed ms; nested spans overlap; D3D API duration is not GPU execution", title);
    for i in 0..N {{
        let calls = after[i].calls - before[i].calls;
        if calls > 0 {{
            crate::log!("profile {{}} {{}} calls={{}} inclusive_elapsed_ms={{:.3}}", title, NAMES[i], calls,
                (after[i].seconds - before[i].seconds) * 1000.0);
        }}
    }}
}}
/// Called only after a successful Render3D_MenuScene and after its scope drops.
pub fn report_completed(view: &str, before: [Stat; N]) {{
    if !owner() {{ return; }}
    let after = snapshot();
    unsafe {{
        if !REPORTED_FIRST {{
            REPORTED_FIRST = true;
            log_stats("first_completed_menu_scene_cumulative", [ZERO; N], after);
        }}
        if !SEEN.iter().any(|s| s == view) {{
            SEEN.push(String::from(view));
            log_stats(view, before, after);
        }}
    }}
}}
'''


def prepare():
    before = hashes(SOURCE)
    if COPY.exists():
        raise SystemExit(f"refusing to replace existing copy: {COPY}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    shutil.copytree(SOURCE, COPY, ignore=shutil.ignore_patterns("target", ".git"))
    for relative, replacements in INJECTIONS.items():
        path = COPY / relative
        text = path.read_text()
        for old, new in replacements:
            count = text.count(old)
            if count != 1:
                raise SystemExit(f"{relative}: expected one exact anchor, found {count}: {old[:80]!r}")
            text = text.replace(old, new, 1)
        path.write_text(text)
    (COPY / "src/profile.rs").write_text(profile_source())
    if before != hashes(SOURCE):
        raise SystemExit("original source hash changed during preparation")
    (OUTPUT / "provenance.json").write_text(json.dumps({
        "source": str(SOURCE), "original_sha256": before,
        "copy": str(COPY), "instrumented_sha256": hashes(COPY),
        "labels": [label for _, label in LABELS],
        "assumptions": ["a2fo-menu-host feature", "host invokes renderer on one graphics owner thread",
                        "inclusive wall-clock scopes; D3D API time does not measure GPU execution"],
    }, indent=2) + "\n")
    print(f"prepared {COPY}; {len(before)} original files hash-verified")


def build():
    if not (COPY / "src/profile.rs").is_file():
        raise SystemExit("run --prepare first")
    record = json.loads((OUTPUT / "provenance.json").read_text())
    if hashes(SOURCE) != record["original_sha256"]:
        raise SystemExit("original source differs from preparation receipt")
    sysroot = subprocess.check_output(["rustup", "run", "1.96.0", "rustc", "--print", "sysroot"], text=True).strip()
    verbose = subprocess.check_output(["rustup", "run", "1.96.0", "rustc", "-vV"], text=True)
    host = next(line.removeprefix("host: ") for line in verbose.splitlines() if line.startswith("host: "))
    linker = Path(sysroot) / "lib/rustlib" / host / "bin/rust-lld"
    if not linker.is_file():
        raise SystemExit(f"missing linker: {linker}")
    env = os.environ.copy()
    env["CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER"] = str(linker)
    env["RUSTFLAGS"] = "-C target-feature=+sse2 -C linker-flavor=lld-link -C link-arg=/nodefaultlib -C link-arg=/entry:DllMain"
    command = ["rustup", "run", "1.96.0", "cargo", "build", "--offline", "--locked",
               "--no-default-features", "--features", "a2fo-menu-host", "--release", "--target", "i686-pc-windows-msvc"]
    print("build command:", " ".join(command), flush=True)
    subprocess.run(command, cwd=COPY, env=env, check=True)
    dll = COPY / "target/i686-pc-windows-msvc/release/render3d.dll"
    candidate = OUTPUT / "A2FORefitScene-profile.dll"
    shutil.copy2(dll, candidate)
    if hashes(SOURCE) != record["original_sha256"]:
        raise SystemExit("original source changed during build")
    print(f"candidate {candidate} sha256={hashlib.sha256(candidate.read_bytes()).hexdigest()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--build", action="store_true")
    args = parser.parse_args()
    if args.prepare:
        prepare()
    else:
        build()


if __name__ == "__main__":
    main()
