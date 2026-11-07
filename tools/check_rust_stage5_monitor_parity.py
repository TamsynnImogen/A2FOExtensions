#!/usr/bin/env python3
"""Compare frozen/current Win32 monitor policy using isolated mock Win32 calls.

The mock header and fixture never invoke real monitor/window APIs. This script
only rewrites first include lines in temporary source copies; production files
and the frozen source stay untouched.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/rust-migration-stage5/monitor-work"
BEFORE = ROOT / "build/rust-migration-stage5/before/core/game_monitor.cpp"
CURRENT = ROOT / "core/game_monitor.cpp"
FIXTURE = ROOT / "tests/rust_stage5_monitor_parity.cpp"
HOST_LIB = ROOT / "build/rust/release/liba2fo_core.a"
WIN_LIB = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"


def call(*args: str, env: dict[str, str] | None = None, output: Path | None = None) -> None:
    print("+", " ".join(args), flush=True)
    if output is None:
        subprocess.run(args, cwd=ROOT, env=env, check=True)
    else:
        with output.open("wb") as stream:
            subprocess.run(args, cwd=ROOT, env=env, check=True, stdout=stream)


def copy_source(source: Path, output: Path, current: bool) -> None:
    content = source.read_text()
    original = '#include "game_monitor.hpp"'
    if content.count(original) != 1:
        raise RuntimeError(f"unexpected monitor include in {source}")
    content = content.replace(original, '#include "mock_monitor.hpp"', 1)
    if current:
        rust_header = '#include "../rust/monitor_ffi.h"'
        if content.count(rust_header) != 1:
            raise RuntimeError(f"unexpected Rust include in {source}")
        content = content.replace(rust_header, f'#include "{ROOT / "rust/monitor_ffi.h"}"', 1)
    output.write_text(content)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("host", "win32"), default="host")
    parser.add_argument("--rust-lib", type=Path)
    parser.add_argument("--run-win32", action="store_true", help="execute mock EXEs headlessly under Wine")
    parser.add_argument("--wine-prefix", type=Path, default=Path("/tmp/a2fo-engine-compat-wine"))
    args = parser.parse_args()
    if args.run_win32 and args.target != "win32":
        parser.error("--run-win32 requires --target win32")
    WORK.mkdir(parents=True, exist_ok=True)
    header = WORK / "mock_monitor.hpp"
    if not header.is_file():
        raise RuntimeError(f"missing isolated mock header: {header}")
    compiler = "g++" if args.target == "host" else "i686-w64-mingw32-g++"
    archive = (args.rust_lib or (HOST_LIB if args.target == "host" else WIN_LIB)).resolve()
    if not archive.is_file():
        raise RuntimeError(f"missing fresh integrated Rust archive: {archive}")
    before_copy = WORK / "before_monitor.cpp"
    current_copy = WORK / "current_monitor.cpp"
    copy_source(BEFORE, before_copy, False)
    copy_source(CURRENT, current_copy, True)
    suffix = "" if args.target == "host" else "_win32"
    ext = "" if args.target == "host" else ".exe"
    before_exe = WORK / f"before_monitor{suffix}{ext}"
    current_exe = WORK / f"current_monitor{suffix}{ext}"
    flags = ["-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Werror"]
    if args.target == "win32":
        flags.append("-static")
    flags += [f"-I{ROOT / 'core'}", f"-I{WORK}"]
    call(compiler, *flags, str(FIXTURE), str(before_copy), "-o", str(before_exe))
    call(compiler, *flags, str(FIXTURE), str(current_copy), str(archive), "-o", str(current_exe))
    if args.target == "win32" and not args.run_win32:
        print("Win32 mock EXEs linked; pass --run-win32 for a headless transcript comparison")
        return
    before_trace = WORK / f"before_monitor{suffix}.trace"
    current_trace = WORK / f"current_monitor{suffix}.trace"
    if args.target == "host":
        call(str(before_exe), output=before_trace)
        call(str(current_exe), output=current_trace)
    else:
        env = dict(os.environ)
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        env["WINEDEBUG"] = "-all"
        env["WINEDLLOVERRIDES"] = (
            "winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d"
        )
        env["WINEPREFIX"] = str(args.wine_prefix)
        call("wine", str(before_exe), env=env, output=before_trace)
        call("wine", str(current_exe), env=env, output=current_trace)
    old = before_trace.read_bytes()
    new = current_trace.read_bytes()
    if old != new:
        import difflib
        diff = difflib.unified_diff(
            old.decode(errors="replace").splitlines(),
            new.decode(errors="replace").splitlines(),
            fromfile=str(before_trace), tofile=str(current_trace), lineterm="",
        )
        print("\n".join(list(diff)[:100]))
        raise SystemExit("monitor parity mismatch")
    required = (b"scenario fallback", b"scenario cap64", b"scenario offscreen_framed",
                b"call SetWindowPos", b"result read_case_sentinel 1")
    for marker in required:
        if marker not in old:
            raise SystemExit(f"monitor fixture did not reach {marker!r}")
    print(f"PASS {args.target}: {len(old.splitlines())} trace lines, sha256={digest(before_trace)}")


if __name__ == "__main__":
    main()
