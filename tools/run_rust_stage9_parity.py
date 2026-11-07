#!/usr/bin/env python3
"""Run selected held stage-nine native comparisons with a private Wine helper."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage9"
GROUPS = {
    "core_parameters": "core/dllmain.cpp",
    "core_shell": "core/dllmain.cpp",
    "core_config": "core/dllmain.cpp",
    "texture_visuals": "modules/A2FOTextureVariants/module.cpp",
    "rgb_runtime": "modules/A2FORGBTextures/module.cpp",
    "video_runtime": "modules/A2FOVideo/module.cpp",
    "craft_runtime": "modules/A2FOCraftIdentity/module.cpp",
    "a1_config": "modules/A1Compat/module.cpp",
    "core_nebula": "core/nebula_renderer.cpp",
    "hybrid_runtime": "modules/A2FOHybridBuild/hybrid_production_runtime.cpp",
    "renderer_options": "core/renderer_options.cpp",
    "module_menu": "core/module_menu.cpp",
    "directional_parse": "modules/A2FODirectionalShields/module.cpp",
    "energy_numeric": "modules/A2FOEnergySystems/module.cpp",
    "directional_module": "modules/A2FODirectionalShields/module.cpp",
    "energy_module": "modules/A2FOEnergySystems/module.cpp",
    "a1_mission_scan": "modules/A1Compat/module.cpp",
    "sol_runtime": "modules/A2FOSOLMaps/module.cpp",
}
LEGACY_MODULES = {"directional_module": "a_directional", "energy_module": "a_energy"}
FRESH_WORK_GROUPS = {"directional_parse", "energy_numeric", "a1_mission_scan", "a1_config", "sol_runtime"}


def fixture_paths(group):
    if group in LEGACY_MODULES:
        suffix = LEGACY_MODULES[group]
        return (ROOT / f"rust/stage8_{suffix}_ffi.h",
                ROOT / f"tests/rust_stage8_{suffix}_native.cpp",
                ROOT / f"tools/check_rust_stage8_{suffix}_parity.py")
    return (ROOT / f"rust/stage9_{group}_ffi.h",
            ROOT / f"tests/rust_stage9_{group}_native.cpp",
            ROOT / f"tools/check_rust_stage9_{group}_parity.py")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--groups", nargs="+", choices=GROUPS, required=True)
    parser.add_argument("--wine-prefix", type=Path,
                        default=Path("/tmp/a2fo-shields-wine-prefix"))
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    prefix = args.wine_prefix.resolve()
    if not (prefix / "system.reg").is_file():
        parser.error("initialized absolute private Wine prefix required")
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    archive = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
    source = ROOT / "rust/a2fo_core/src"
    lib = source / "lib.rs"
    # Unregistered files may still be under construction by other workers.
    # Every registered crate input and selected native workflow is held.
    registered = {source / f"{name}.rs" for name in
                  re.findall(r"^mod\s+(\w+)\s*;", lib.read_text(), re.MULTILINE)}
    registered.update((lib, source.parent / "Cargo.toml", source.parent / "Cargo.lock"))
    for path in registered:
        if path.stat().st_mtime_ns > archive.stat().st_mtime_ns:
            raise RuntimeError(f"stale canonical archive: {path}")
    inputs = {archive, Path(__file__).resolve(), *registered}
    inputs.update((ROOT / "rust").glob("*_ffi.h"))
    for group in args.groups:
        native_source = ROOT / GROUPS[group]
        inputs.add(native_source)
        inputs.update(fixture_paths(group))
        inputs.update(path for path in native_source.parent.iterdir()
                      if path.is_file() and path.suffix in (".cpp", ".h", ".hpp", ".inl", ".S"))
    inputs.update(path for path in (ROOT / "sdk").rglob("*") if path.is_file())
    if set(args.groups) & {"directional_parse", "directional_module", "energy_numeric", "energy_module"}:
        inputs.update((ROOT / "rust/stage9_directional_parse_ffi.h",
                       ROOT / "rust/stage9_energy_numeric_ffi.h"))
    if "craft_runtime" in args.groups:
        inputs.add(ROOT / "rust/stage9_craft_sprite_ffi.h")
    initial = {str(path): sha(path) for path in sorted(inputs)}
    receipt = {"inputs": initial, "groups": [], "passed": False}

    def save():
        (out / "results.json").write_text(json.dumps(receipt, indent=2) + "\n")

    def held():
        if any(sha(Path(path)) != value for path, value in initial.items()):
            raise RuntimeError("held native comparison inputs changed")

    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(WINEPREFIX=str(prefix), A2FO_RUST_WINEPREFIX=str(prefix),
               A2FO_RUST_PREFIX=str(prefix), A2FO_RUST_WIN_LIB=str(archive),
               LIBGL_ALWAYS_SOFTWARE="1", WINEDEBUG="-all",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    helper = out / "keepalive.cpp"
    helper.write_text('#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n'
                      '#include <cstdio>\nint main(int n,char**v){if(n!=2)return 2;'
                      'std::puts("ready");std::fflush(stdout);'
                      'for(unsigned i=0;i<28800;++i){'
                      'if(GetFileAttributesA(v[1])!=INVALID_FILE_ATTRIBUTES)return 0;'
                      'Sleep(250);}return 3;}\n')
    executable = out / "keepalive.exe"
    subprocess.run(["i686-w64-mingw32-g++", "-O2", "-static", str(helper),
                    "-o", str(executable)], cwd=ROOT, check=True, timeout=180)
    marker = f"a2fo-stage9-stop-{os.getpid()}"
    stop = prefix / "drive_c" / marker
    stop.unlink(missing_ok=True)
    save()
    with (out / "keepalive.log").open("wb") as log:
        keep = subprocess.Popen(["wine", str(executable), "C:\\" + marker],
                                cwd=ROOT, env=env, stdout=log, stderr=log)
        try:
            started = time.monotonic()
            while b"ready" not in (out / "keepalive.log").read_bytes():
                if keep.poll() is not None or time.monotonic() - started > 360:
                    raise RuntimeError("headless private Wine helper failed startup")
                time.sleep(0.5)
            for group in args.groups:
                held()
                started = time.monotonic()
                with (out / f"{group}.log").open("wb") as log:
                    command = ["python3", str(fixture_paths(group)[2])]
                    if group in FRESH_WORK_GROUPS:
                        command.extend(("--work-dir", str(out / f"{group}-work")))
                    code = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                          stderr=log, timeout=2400).returncode
                receipt["groups"].append({"name": group, "exit": code,
                                          "seconds": round(time.monotonic() - started, 2)})
                save()
                print(receipt["groups"][-1], flush=True)
                held()
                if code:
                    raise RuntimeError(f"{group} failed; inspect its log in {out}")
            receipt["passed"] = True
            save()
        finally:
            stop.touch()
            try:
                keep.wait(timeout=30)
            except subprocess.TimeoutExpired:
                keep.terminate()
                keep.wait(timeout=30)
            stop.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
