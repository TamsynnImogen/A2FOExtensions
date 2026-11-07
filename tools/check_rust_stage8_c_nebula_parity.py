#!/usr/bin/env python3
"""Compile the actual frozen/current NebulaRenderer module against one mock."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/rust-migration-stage8/nebula-work"
BEFORE = ROOT / "build/rust-migration-stage8/before"


def compile_case(label: str, source: Path, config: Path, rust_lib: Path) -> Path:
    output = WORK / f"{label}.exe"
    command = [
        "i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall",
        "-Wextra", "-Werror", "-DNDEBUG", "-static", "-static-libgcc",
        "-static-libstdc++", f'-DA2FO_FIXTURE_SOURCE="{source}"',
        str(ROOT / "tests/rust_stage8_c_nebula_native.cpp"), str(config),
        str(ROOT / "modules/A2FONebulaRenderer/thiscall_bridge.S"),
        str(rust_lib), "-o", str(output),
    ]
    subprocess.run(command, cwd=ROOT, check=True)
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--rust-lib", type=Path,
                        default=Path(os.environ.get(
                            "A2FO_RUST_WIN_LIB",
                            ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a")))
    parser.add_argument("--wine-prefix", type=Path,
                        default=(Path(os.environ["A2FO_RUST_WINEPREFIX"])
                                 if "A2FO_RUST_WINEPREFIX" in os.environ else None))
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    frozen = BEFORE / "modules/A2FONebulaRenderer/module.cpp"
    expected = json.loads((BEFORE.parent / "before-hashes.json").read_text())
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()
    if digest(frozen) != expected["modules/A2FONebulaRenderer/module.cpp"]:
        raise RuntimeError("frozen NebulaRenderer source drift")
    pinned = [frozen, ROOT / "modules/A2FONebulaRenderer/module.cpp",
              ROOT / "rust/a2fo_core/src/stage8_c_nebula.rs",
              ROOT / "rust/stage8_c_nebula_ffi.h",
              ROOT / "tests/rust_stage8_c_nebula_native.cpp",
              ROOT / "tests/rust_stage8_c_nebula_core_mock.cpp",
              ROOT / "modules/A2FONebulaRenderer/art_texture_suffix_config.cpp",
              BEFORE / "modules/A2FONebulaRenderer/art_texture_suffix_config.cpp",
              Path(__file__).resolve(), args.rust_lib.resolve()]
    initial = {str(path): digest(path) for path in pinned}
    subprocess.run([
        "i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall",
        "-Wextra", "-Werror", "-shared", "-static",
        "-static-libgcc", "-static-libstdc++",
        str(ROOT / "tests/rust_stage8_c_nebula_core_mock.cpp"),
        "-o", str(WORK / "A2FOExtensions.dll"),
    ], cwd=ROOT, check=True)
    before = compile_case(
        "before", BEFORE / "modules/A2FONebulaRenderer/module.cpp",
        BEFORE / "modules/A2FONebulaRenderer/art_texture_suffix_config.cpp",
        args.rust_lib,
    )
    current = compile_case(
        "current", ROOT / "modules/A2FONebulaRenderer/module.cpp",
        ROOT / "modules/A2FONebulaRenderer/art_texture_suffix_config.cpp",
        args.rust_lib,
    )
    if args.build_only:
        print(f"built {before} and {current}")
        return 0
    if not args.wine_prefix:
        parser.error("--wine-prefix or A2FO_RUST_WINEPREFIX is required")
    traces = []
    env = dict(os.environ, WINEPREFIX=str(args.wine_prefix.resolve()),
               WINEDEBUG="-all", LIBGL_ALWAYS_SOFTWARE="1",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    # Both executables see the same empty pathname. Reusing WORK directly
    # allowed files from the previous executable/run to affect texture probes.
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=WORK) as temporary:
        case = Path(temporary) / "case"
        for executable in (before, current):
            case.mkdir()
            trace = WORK / f"{executable.stem}.stdout"
            errors = WORK / f"{executable.stem}.stderr"
            with trace.open("wb") as out, errors.open("wb") as err:
                subprocess.run(
                    ["wine", str(executable)], cwd=case, env=env,
                    stdout=out, stderr=err, timeout=180, check=True,
                )
            traces.append(trace.read_bytes())
            shutil.rmtree(case)
    if not traces[0] or b"slash-priority:" not in traces[0]:
        raise RuntimeError("missing NebulaRenderer regression trace")
    if any(digest(path) != initial[str(path)] for path in pinned):
        raise RuntimeError("NebulaRenderer parity input drift")
    if traces[0] != traces[1]:
        (WORK / "before.trace").write_bytes(traces[0])
        (WORK / "current.trace").write_bytes(traces[1])
        raise SystemExit("NebulaRenderer frozen/current path trace differs")
    (WORK / "parity.trace").write_bytes(traces[0])
    (WORK / "parity.json").write_text(json.dumps({
        "inputs": initial, "equal": True, "lines": len(traces[0].splitlines()),
        "sha256": hashlib.sha256(traces[0]).hexdigest(),
    }, indent=2) + "\n")
    print(f"NebulaRenderer equal trace: {len(traces[0].splitlines())} lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
