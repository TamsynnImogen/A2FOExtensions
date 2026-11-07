#!/usr/bin/env python3
"""Immutable frozen/current native comparison for unsupported D3D8 methods."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "build/rust-migration-stage10/unsupported"
SOURCE = Path("renderer/d3d8_compat/game/unsupported.cpp")
FROZEN = ROOT / "build/rust-migration-stage8/before" / SOURCE
CURRENT = ROOT / SOURCE
FIXTURE = ROOT / "tests/rust_stage10_unsupported_native.cpp"
PRIVATE = ROOT / "tests/rust_stage10_unsupported_private.hpp"
FFI = ROOT / "rust/stage8_b_unsupported_ffi.h"
RUST = ROOT / "rust/a2fo_core/src/stage8_b_unsupported.rs"
HASHES = ROOT / "build/rust-migration-stage8/before-hashes.json"
CHECKER = Path(__file__).resolve()
ROW = re.compile(rb"(\d{2})\|([0-3])\|[0-9a-fA-F]{8}\|[01]\|[12]\|[A-Za-z]+ unsupported in experimental backend")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(paths):
    return {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p): sha(p)
            for p in paths}


def record(work, attempt):
    (work / "attempt.json").write_text(json.dumps(attempt, indent=2) + "\n")


def run(work, attempt, label, command, *, env=None, timeout=180):
    stdout_path = work / f"{label}.stdout.log"
    stderr_path = work / f"{label}.stderr.log"
    step = {"label": label, "command": command,
            "stdout": stdout_path.name, "stderr": stderr_path.name}
    attempt["steps"].append(step)
    record(work, attempt)
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        try:
            completed = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                       stderr=stderr, timeout=timeout, check=False)
            step["exit_status"] = completed.returncode
        except subprocess.TimeoutExpired:
            step["exit_status"] = "timeout"
            record(work, attempt)
            raise RuntimeError(f"{label} timed out; see {stdout_path} and {stderr_path}")
    step["stdout_sha256"] = sha(stdout_path)
    step["stderr_sha256"] = sha(stderr_path)
    record(work, attempt)
    if step["exit_status"] != 0:
        raise RuntimeError(f"{label} exit {step['exit_status']}; see {stderr_path}")
    return stdout_path.read_bytes().replace(b"\r\n", b"\n")


def validate_trace(raw, label):
    lines = raw.splitlines()
    if len(lines) != 94 or lines[0] != b"ready:unsupported" or lines[-1] != b"completed:unsupported":
        raise RuntimeError(f"{label}: readiness/completion markers or row count invalid")
    seen = set()
    for line in lines[1:-1]:
        match = ROW.fullmatch(line)
        if match is None:
            raise RuntimeError(f"{label}: malformed case row {line!r}")
        key = (int(match[1]), int(match[2]))
        if key[0] >= 23 or key in seen:
            raise RuntimeError(f"{label}: duplicate/invalid case {key}")
        seen.add(key)
    expected = {(operation, mode) for operation in range(23) for mode in range(4)}
    if seen != expected:
        raise RuntimeError(f"{label}: missing cases {sorted(expected - seen)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--rust-lib", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--wine-prefix", type=Path)
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    work = args.work_dir.resolve()
    if work.parent != BASE.resolve() or work.exists():
        parser.error("--work-dir must be a new direct child of the unsupported slice")
    if not args.compile_only and args.wine_prefix is None:
        parser.error("--wine-prefix required for parity execution")
    if not archive.is_file():
        parser.error(f"Rust archive missing: {archive}")
    work.mkdir(parents=True, exist_ok=False)
    attempt = {"status": "started", "steps": [], "snapshots": {}}
    record(work, attempt)
    try:
        frozen_hash = json.loads(HASHES.read_text())[str(SOURCE)]
        if sha(FROZEN) != frozen_hash:
            raise RuntimeError("frozen stage8 source hash mismatch")
        inputs = [FROZEN, CURRENT, FIXTURE, PRIVATE, FFI, RUST, HASHES, CHECKER, archive]
        attempt["frozen_authenticated_sha256"] = frozen_hash
        attempt["snapshots"]["before_compile"] = snapshot(inputs)
        record(work, attempt)
        exes = []
        generated = []
        for label, source in (("before", FROZEN), ("current", CURRENT)):
            body = source.read_text().replace(
                '#include "private.hpp"', f'#include "{PRIVATE}"', 1)
            if label == "current":
                body = body.replace(
                    '#include "../../../rust/stage8_b_unsupported_ffi.h"',
                    f'#include "{FFI}"', 1)
            code = work / f"{label}.cpp"
            code.write_text(body)
            generated.append(code)
            attempt["snapshots"][f"generated_{label}"] = snapshot(generated)
            record(work, attempt)
            exe = work / f"{label}.exe"
            command = ["i686-w64-mingw32-g++", "-DWIN32_LEAN_AND_MEAN",
                       "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                       "-Werror", "-static", str(FIXTURE), str(code)]
            if label == "current":
                command.append(str(archive))
            command += ["-o", str(exe)]
            run(work, attempt, f"build-{label}", command)
            exes.append(exe)
            attempt["snapshots"][f"compiled_{label}"] = snapshot(generated + exes)
            record(work, attempt)
        attempt["snapshots"]["after_compile_inputs"] = snapshot(inputs)
        if attempt["snapshots"]["before_compile"] != attempt["snapshots"]["after_compile_inputs"]:
            raise RuntimeError("source/dependency/archive changed during compilation")
        attempt["compiler"] = "i686-w64-mingw32-g++ GNU++17/O2/Werror"
        attempt["methods"] = 23
        attempt["cases"] = 92
        if not args.compile_only:
            env = os.environ.copy()
            env.pop("DISPLAY", None)
            env.pop("WAYLAND_DISPLAY", None)
            env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG="-all",
                       LIBGL_ALWAYS_SOFTWARE="1", GALLIUM_DRIVER="llvmpipe",
                       MESA_LOADER_DRIVER_OVERRIDE="llvmpipe",
                       WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d")
            logs = []
            for label, exe in (("before", exes[0]), ("current", exes[1])):
                raw = run(work, attempt, f"run-{label}", ["wine", str(exe)],
                          env=env, timeout=90)
                validate_trace(raw, label)
                logs.append(raw)
            if logs[0] != logs[1]:
                raise RuntimeError("frozen/current native transcripts differ")
            attempt["transcript_sha256"] = hashlib.sha256(logs[0]).hexdigest()
            attempt["snapshots"]["after_execution_inputs"] = snapshot(inputs)
            attempt["snapshots"]["after_execution_artifacts"] = snapshot(generated + exes)
            if attempt["snapshots"]["before_compile"] != attempt["snapshots"]["after_execution_inputs"]:
                raise RuntimeError("source/dependency/archive changed during execution")
            if attempt["snapshots"]["compiled_current"] != attempt["snapshots"]["after_execution_artifacts"]:
                raise RuntimeError("generated source/executable changed during execution")
        attempt["status"] = "compile-only" if args.compile_only else "parity-pass"
        record(work, attempt)
        (work / "completion.json").write_text(json.dumps(attempt, indent=2) + "\n")
        print(f"unsupported: {attempt['cases']} cases, {attempt['status']}")
    except Exception as error:
        attempt["status"] = "failed"
        attempt["error"] = str(error)
        record(work, attempt)
        raise


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
