#!/usr/bin/env python3
"""Build identical deterministic policy transcripts before and after Rust migration."""
import argparse
import hashlib
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def run(command, env=None):
    # A regular file prevents Wine services inheriting a pipe from keeping the
    # parent waiting after the fixture exits.
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                stderr=stderr, timeout=60)
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().decode("utf-8", "replace")
        errors = stderr.read().decode("utf-8", "replace")
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(map(str, command))}\n"
                           f"stdout:\n{output}\nstderr:\n{errors}")
    return output


def build(output, sources, includes, archive=None, windows=False):
    compiler = "i686-w64-mingw32-g++" if windows else "g++"
    command = [compiler, "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-pthread"]
    if windows:
        command.append("-static")
    for include in includes:
        command.extend(["-I", str(include)])
    command.extend([str(ROOT / "tests/rust_migration_parity.cpp")])
    command.extend(map(str, sources))
    if archive:
        command.append(str(archive))
        # The no_std Rust archive uses only the compiler-linked CRT.
    command.extend(["-o", str(output)])
    run(command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=pathlib.Path,
                        default=ROOT / "build/rust-migration-stage1/before")
    parser.add_argument("--archive", type=pathlib.Path,
                        help="defaults to the selected target's release static library")
    parser.add_argument("--windows", action="store_true",
                        help="compare 32-bit MinGW builds headlessly under Wine")
    parser.add_argument("--wine-prefix", type=pathlib.Path,
                        default=pathlib.Path("/tmp/a2fo-engine-compat-wine"))
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    archive = (args.archive or ROOT / ("build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
               if args.windows else "build/rust/release/liba2fo_core.a")).resolve()
    units = ["core/module_policy.cpp", "core/builtin_state.cpp",
             "modules/A2FOEnergySystems/resupply_policy.cpp"]
    for source in units:
        if not (baseline / source).is_file():
            parser.error(f"baseline source missing: {baseline / source}")
    if not archive.is_file():
        parser.error(f"Rust host archive missing: {archive}")
    # Baseline lacks two unchanged headers. Its own builtin_state.hpp is first.
    # No baseline file is written or modified.
    old_includes = [baseline / "core", baseline / "modules/A2FOEnergySystems",
                    ROOT / "core", ROOT / "modules/A2FOEnergySystems"]
    new_includes = [ROOT / "core", ROOT / "modules/A2FOEnergySystems"]
    with tempfile.TemporaryDirectory(prefix="a2fo-rust-parity-") as directory:
        directory = pathlib.Path(directory)
        suffix = ".exe" if args.windows else ""
        old_exe, new_exe = directory / ("before" + suffix), directory / ("rust" + suffix)
        build(old_exe, [baseline / source for source in units], old_includes, windows=args.windows)
        build(new_exe, [ROOT / source for source in units], new_includes, archive, args.windows)
        env = os.environ.copy()
        runner = []
        if args.windows:
            for key in ("DISPLAY", "WAYLAND_DISPLAY"):
                env.pop(key, None)
            env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG="-all",
                       WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d")
            runner = ["wine"]
        before = run(runner + [str(old_exe)], env).encode()
        current = run(runner + [str(new_exe)], env).encode()
        old_lines, new_lines = before.splitlines(), current.splitlines()
        print(f"cases: baseline={len(old_lines)} rust={len(new_lines)}")
        print(f"baseline sha256: {hashlib.sha256(before).hexdigest()}")
        print(f"rust sha256:     {hashlib.sha256(current).hexdigest()}")
        if before != current:
            for index in range(max(len(old_lines), len(new_lines))):
                left = old_lines[index] if index < len(old_lines) else b"<missing>"
                right = new_lines[index] if index < len(new_lines) else b"<missing>"
                if left != right:
                    label = left.split(b" ", 1)[0].decode("ascii", "replace")
                    print(f"first mismatch: line {index + 1} case {label}", file=sys.stderr)
                    print(f"baseline: {left[:256].decode('ascii', 'replace')}", file=sys.stderr)
                    print(f"rust:     {right[:256].decode('ascii', 'replace')}", file=sys.stderr)
                    return 1
        print("Rust migration parity: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
