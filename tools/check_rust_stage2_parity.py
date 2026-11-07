#!/usr/bin/env python3
"""Compare stage-two policies and texture bytes against the frozen C++ sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
from check_rust_migration_parity import run

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {
    "hybrid": (["modules/A2FOHybridBuild/hybrid_production.cpp", "modules/A2FOHybridBuild/build_submenu_config.cpp"],
               ["modules/A2FOHybridBuild"], "tests/rust_hybrid_parity.cpp"),
    "stores": (["modules/A2FOEnergySystems/energy_systems.cpp", "modules/A2FOWreckage/wreckage_policy.cpp"],
               ["modules/A2FOEnergySystems", "modules/A2FOWreckage"], "tests/rust_stores_parity.cpp"),
    "textures": (["renderer/textures/dds.cpp", "renderer/textures/upload.cpp"],
                 ["renderer/textures"], "tests/rust_textures_parity.cpp"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--group", choices=[*GROUPS, "all"], default="all")
    parser.add_argument("--windows", action="store_true")
    parser.add_argument("--baseline", type=Path, default=ROOT / "build/rust-migration-stage2/before")
    parser.add_argument("--wine-prefix", type=Path, default=Path("/tmp/a2fo-engine-compat-wine"))
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()
    target = "i686-pc-windows-gnu/release" if args.windows else "release"
    archive = ROOT / "build/rust" / target / "liba2fo_core.a"
    inputs = [* (ROOT / "rust/a2fo_core/src").glob("*.rs"),
              ROOT / "rust/a2fo_core/Cargo.toml", ROOT / "rust/a2fo_core/Cargo.lock"]
    if not archive.is_file() or any(p.stat().st_mtime_ns > archive.stat().st_mtime_ns for p in inputs):
        parser.error(f"Rust archive is missing or stale; build the {target} static library first")
    env = os.environ.copy()
    runner = []
    if args.windows:
        for name in ("DISPLAY", "WAYLAND_DISPLAY"):
            env.pop(name, None)
        env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG="-all",
                   WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d")
        runner = ["wine"]
    groups = list(GROUPS) if args.group == "all" else [args.group]
    results = []
    for name in groups:
        if name == "textures" and not args.windows:
            print("textures: skipped on Linux; run --windows for the existing Windows header contract")
            continue
        units, includes, fixture = GROUPS[name]
        with tempfile.TemporaryDirectory(prefix="a2fo-rust-stage2-") as temp:
            outputs = []
            for old in (True, False):
                base = args.baseline.resolve() if old else ROOT
                exe = Path(temp) / (("before" if old else "rust") + (".exe" if args.windows else ""))
                command = ["i686-w64-mingw32-g++" if args.windows else "g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-pthread"]
                if args.windows:
                    command.append("-static")
                for directory in includes:
                    command.extend(["-I", str(base / directory)])
                command.append(str(ROOT / fixture))
                command.extend(str(base / unit) for unit in units)
                if not old:
                    command.append(str(archive))
                command.extend(["-o", str(exe)])
                run(command)
                outputs.append(run(runner + [str(exe)], env).encode())
            before, current = outputs
            if before != current:
                a, b = before.splitlines(), current.splitlines()
                for i in range(max(len(a), len(b))):
                    left = a[i] if i < len(a) else b"<missing>"
                    right = b[i] if i < len(b) else b"<missing>"
                    if left != right:
                        print(f"{name} mismatch at line {i+1}:\nbaseline: {left[:600]!r}\nrust:     {right[:600]!r}", file=sys.stderr)
                        return 1
            result = {"group": name, "cases": len(current.splitlines()),
                      "sha256": hashlib.sha256(current).hexdigest(), "equal": True}
            results.append(result)
            print(f"{name}: {result['cases']} cases, SHA256 {result['sha256']}, PASS", flush=True)
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps({"windows": args.windows, "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(), "results": results}, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(error, file=sys.stderr)
        sys.exit(2)
