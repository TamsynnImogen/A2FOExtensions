#!/usr/bin/env python3
"""Replay the pending stage-eight native ports against held canonical archives."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage8"
GROUPS = {
    "hybrid-state": "hybrid_state",
    "a1-officer": "a1_officer",
    "turrets": "a_turrets",
    "nebula": "c_nebula",
    "texture": "texture_runtime",
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--groups", nargs="+", choices=GROUPS, default=list(GROUPS))
    parser.add_argument("--wine-prefix", type=Path,
                        default=Path("/tmp/a2fo-shields-wine-prefix"))
    parser.add_argument("--output-dir", type=Path,
                        default=STAGE / "resume-20261026/native")
    args = parser.parse_args()
    prefix = args.wine_prefix.resolve()
    if not (prefix / "system.reg").is_file():
        parser.error("an initialized private Wine prefix is required")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
    pinned = {archive, ROOT / "rust/a2fo_core/src/lib.rs", Path(__file__).resolve()}
    # The static archive includes every registered Rust dependency, so all
    # crate inputs must be held and fresh, including transitive numeric helpers.
    crate_inputs = set((ROOT / "rust/a2fo_core/src").glob("*.rs"))
    crate_inputs.update((ROOT / "rust/a2fo_core/Cargo.toml",
                         ROOT / "rust/a2fo_core/Cargo.lock"))
    for path in crate_inputs:
        if path.stat().st_mtime_ns > archive.stat().st_mtime_ns:
            raise RuntimeError(f"stale canonical archive: {path}")
    pinned.update(crate_inputs)
    pinned.update((ROOT / "rust").glob("*_ffi.h"))
    sources = {
        "hybrid-state": ["modules/A2FOHybridBuild/hybrid_production_runtime.cpp",
                         "modules/A2FOHybridBuild/hybrid_production.cpp"],
        "a1-officer": ["modules/A1Compat/module.cpp"],
        "turrets": ["modules/A2FOTurrets/module.cpp",
                    "modules/A2FOTurrets/turret_math.cpp",
                    "modules/A2FOTurrets/turret_combat.cpp"],
        "nebula": ["modules/A2FONebulaRenderer/module.cpp",
                   "modules/A2FONebulaRenderer/art_texture_suffix_config.cpp"],
        "texture": ["modules/A2FOTextureVariants/module.cpp",
                    "modules/A2FOTextureVariants/texture_variants.cpp"],
    }
    for group in args.groups:
        suffix = GROUPS[group]
        rust = ROOT / f"rust/a2fo_core/src/stage8_{suffix}.rs"
        if rust.stat().st_mtime_ns > archive.stat().st_mtime_ns:
            raise RuntimeError(f"stale canonical archive: {rust}")
        pinned.update((rust, ROOT / f"rust/stage8_{suffix}_ffi.h",
                       ROOT / f"tools/check_rust_stage8_{suffix}_parity.py"))
        fixture_suffix = suffix if group != "turrets" else "a_turrets"
        pinned.add(ROOT / f"tests/rust_stage8_{fixture_suffix}_native.cpp")
        for source in sources[group]:
            pinned.update((ROOT / source, STAGE / "before" / source))
    initial = {str(path): sha(path) for path in sorted(pinned)}
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(WINEPREFIX=str(prefix), A2FO_RUST_WINEPREFIX=str(prefix),
               A2FO_RUST_WIN_LIB=str(archive), WINEDEBUG="-all",
               LIBGL_ALWAYS_SOFTWARE="1",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    helper = output / "keepalive.cpp"
    helper.write_text(
        '#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include <cstdio>\n'
        'int main(int argc,char**argv){if(argc!=2)return 2;'
        'std::puts("ready");std::fflush(stdout);'
        'for(unsigned i=0;i<28800;++i){'
        'if(GetFileAttributesA(argv[1])!=INVALID_FILE_ATTRIBUTES)return 0;'
        'Sleep(250);}return 3;}\n')
    executable = output / "keepalive.exe"
    subprocess.run(["i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall",
                    "-Wextra", "-Werror", "-static", str(helper), "-o",
                    str(executable)], cwd=ROOT, check=True, timeout=180)
    marker = f"a2fo-stage8-resume-stop-{os.getpid()}"
    stop = prefix / "drive_c" / marker
    stop.unlink(missing_ok=True)
    receipt = {"started_utc": datetime.now(timezone.utc).isoformat(),
               "inputs": initial, "groups": [], "passed": False}

    def save():
        (output / "results.json").write_text(json.dumps(receipt, indent=2) + "\n")

    save()
    with (output / "keepalive.log").open("wb") as log:
        keep = subprocess.Popen(["wine", str(executable), "C:\\" + marker],
                                cwd=ROOT, env=env, stdout=log, stderr=log)
        try:
            start = time.monotonic()
            while b"ready" not in (output / "keepalive.log").read_bytes():
                if keep.poll() is not None or time.monotonic() - start > 360:
                    raise RuntimeError("private headless Wine helper failed startup")
                time.sleep(0.5)
            for group in args.groups:
                if any(sha(Path(path)) != value for path, value in initial.items()):
                    raise RuntimeError("held native parity inputs changed")
                start = time.monotonic()
                command = ["python3", f"tools/check_rust_stage8_{GROUPS[group]}_parity.py"]
                with (output / f"{group}.log").open("wb") as out:
                    result = subprocess.run(command, cwd=ROOT, env=env,
                                            stdout=out, stderr=out, timeout=2400)
                row = {"name": group, "exit": result.returncode,
                       "seconds": round(time.monotonic() - start, 2)}
                receipt["groups"].append(row)
                save()
                print(json.dumps(row), flush=True)
                if result.returncode:
                    raise RuntimeError(f"{group} failed; see {output / (group + '.log')}")
            if any(sha(Path(path)) != value for path, value in initial.items()):
                raise RuntimeError("held native parity inputs changed")
            receipt["passed"] = True
            receipt["completed_utc"] = datetime.now(timezone.utc).isoformat()
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
