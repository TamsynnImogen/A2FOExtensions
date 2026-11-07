#!/usr/bin/env python3
"""Headless frozen/current core differential; never starts a game or input device."""
from __future__ import annotations
import argparse
import difflib
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage5"
WORK = STAGE / "core-work"
TEST = ROOT / "tests/rust_stage5_core_lifecycle_parity.cpp"
ARCHIVE = ROOT / "build/rust/release/liba2fo_core.a"
CASES = {
    "roots": ("extension_roots.cpp", "A2FO_CORE_ROOTS", True),
    "hook": ("hook.cpp", "A2FO_CORE_HOOK", False),
    "loader": ("module_loader.cpp", "A2FO_CORE_LOADER", True),
    "builtin": ("builtin_modules.cpp", "A2FO_CORE_BUILTIN", True),
}

def command(case: str, side: str, archive: Path, exe: Path) -> list[str]:
    source, define, windows = CASES[case]
    cpp = (STAGE / "before/core" if side == "before" else ROOT / "core") / source
    cmd = ["g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
    if windows:
        cmd.append("-D_WIN32")
    if case == "hook" and side == "current":
        cpp=ROOT / "tests/native_reference/adapters/core/hook.cpp"
        cmd+=[str(ROOT / "tests/rust_stage16_hook_mock_backend.cpp")]
    if case == "builtin":
        cmd += ["-D__cdecl=", "-D__declspec(x)="]
    cmd += ["-D" + define, "-I", str(ROOT / "core"), "-I", str(WORK / "shim"),
            str(TEST), str(cpp)]
    if case == "builtin":
        base = STAGE / "before/core" if side == "before" else ROOT / "core"
        cmd += [str(ROOT / "tests/builtin_startup_stubs.cpp"),
                str(base / "builtin_startup_modules.cpp"),
                str(base / "builtin_startup_policy.cpp"),
                str(STAGE / "before/core/builtin_state.cpp")]
    if side == "current" or case == "builtin":
        cmd += [str(archive), "-ldl", "-lpthread"]
    return cmd + ["-o", str(exe)]

def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=180, check=False)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cases", nargs="*", choices=list(CASES), default=list(CASES))
    parser.add_argument("--archive", type=Path, default=ARCHIVE)
    args = parser.parse_args()
    archive = args.archive.resolve()
    if not archive.is_file():
        parser.error(f"integrated Rust archive missing: {archive}")
    WORK.mkdir(parents=True, exist_ok=True)
    failed = False
    for case in args.cases:
        outputs: dict[str, str] = {}
        for side in ("before", "current"):
            exe = WORK / f"{case}-{side}"
            build = command(case, side, archive, exe)
            (WORK / f"{case}-{side}.command").write_text(shlex.join(build) + "\n")
            result = run(build)
            (WORK / f"{case}-{side}.build.stderr").write_text(result.stderr)
            if result.returncode:
                print(f"{case}/{side}: build exit {result.returncode}\n{result.stderr[-4000:]}", file=sys.stderr)
                return 2
            result = run([str(exe)])
            (WORK / f"{case}-{side}.txt").write_text(result.stdout)
            (WORK / f"{case}-{side}.run.stderr").write_text(result.stderr)
            if result.returncode:
                print(f"{case}/{side}: run exit {result.returncode}\n{result.stderr[-4000:]}", file=sys.stderr)
                return 2
            outputs[side] = result.stdout
        if outputs["before"] != outputs["current"]:
            failed = True
            diff = "".join(difflib.unified_diff(outputs["before"].splitlines(True),
                 outputs["current"].splitlines(True), fromfile="before", tofile="current"))
            (WORK / f"{case}.diff").write_text(diff)
            print(f"{case}: MISMATCH (see {WORK / (case + '.diff')})")
        else:
            print(f"{case}: exact {len(outputs['before'].splitlines())} rows")
    return int(failed)

if __name__ == "__main__":
    raise SystemExit(main())
