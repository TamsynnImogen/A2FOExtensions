#!/usr/bin/env python3
"""Build and compare frozen/current mission backdrop against headless Win32 mocks.

No native game, D3D device, window, monitor, or input APIs are called. Only the
first include lines change in isolated copies so both original algorithms run
against the same mock operations and synthetic 32-bit memory.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/rust-migration-stage5/backdrop-work"
BEFORE = ROOT / "build/rust-migration-stage5/before/modules/A2FORefitMenus/mission_backdrop.cpp"
CURRENT = ROOT / "modules/A2FORefitMenus/mission_backdrop.cpp"
FIXTURE = ROOT / "tests/rust_stage5_backdrop_parity.cpp"
ARCHIVE = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"


def run(*args: str, output: Path | None = None, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(args), flush=True)
    if output is None:
        subprocess.run(args, cwd=ROOT, env=env, check=True)
    else:
        with output.open("wb") as stream:
            subprocess.run(args, cwd=ROOT, env=env, stdout=stream, check=True)


def copied_source(source: Path, destination: Path, current: bool) -> None:
    content = source.read_text()
    for old, new in (
        ('#include "mission_backdrop.hpp"', '#include "mock_backdrop.hpp"'),
        ('#include "menu_scene.hpp"', ''),
        ('#include <d3d8.h>', ''),
    ):
        if content.count(old) != 1:
            raise RuntimeError(f"unexpected include in {source}: {old}")
        content = content.replace(old, new, 1)
    if current:
        old = '#include "../../rust/backdrop_ffi.h"'
        if content.count(old) != 1:
            raise RuntimeError(f"unexpected Rust include in {source}")
        content = content.replace(old, f'#include "{ROOT / "rust/backdrop_ffi.h"}"', 1)
    destination.write_text(content)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-lib", type=Path, default=ARCHIVE)
    parser.add_argument("--run-win32", action="store_true", help="run both mock EXEs headlessly under Wine")
    parser.add_argument("--wine-prefix", type=Path, default=Path("/tmp/a2fo-engine-compat-wine"))
    args = parser.parse_args()
    if not (WORK / "mock_backdrop.hpp").is_file():
        raise RuntimeError("missing isolated mock_backdrop.hpp")
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        raise RuntimeError(f"missing fresh integrated Rust archive: {archive}")
    before_copy = WORK / "before_backdrop.cpp"
    current_copy = WORK / "current_backdrop.cpp"
    copied_source(BEFORE, before_copy, False)
    copied_source(CURRENT, current_copy, True)
    flags = ["-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Werror", "-static",
             f"-I{ROOT / 'modules/A2FORefitMenus'}", f"-I{WORK}"]
    before_exe = WORK / "before_backdrop_win32.exe"
    current_exe = WORK / "current_backdrop_win32.exe"
    compiler = "i686-w64-mingw32-g++"
    run(compiler, *flags, str(FIXTURE), str(before_copy), "-o", str(before_exe))
    run(compiler, *flags, str(FIXTURE), str(current_copy), str(archive), "-o", str(current_exe))
    if not args.run_win32:
        print("Win32 mock EXEs linked; pass --run-win32 for headless trace parity")
        return
    env = dict(os.environ)
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env["WINEDEBUG"] = "-all"
    env["WINEDLLOVERRIDES"] = (
        "winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d"
    )
    env["WINEPREFIX"] = str(args.wine_prefix)
    old_trace = WORK / "before_backdrop_win32.trace"
    new_trace = WORK / "current_backdrop_win32.trace"
    run("wine", str(before_exe), env=env, output=old_trace)
    run("wine", str(current_exe), env=env, output=new_trace)
    old = old_trace.read_bytes()
    new = new_trace.read_bytes()
    if old != new:
        diff = difflib.unified_diff(old.decode(errors="replace").splitlines(),
                                    new.decode(errors="replace").splitlines(),
                                    fromfile=str(old_trace), tofile=str(new_trace), lineterm="")
        print("\n".join(list(diff)[:120]))
        raise SystemExit("backdrop parity mismatch")
    required = (b"initialize fo_chain 1", b"call original_render",
                b"frame captured 3 2 6", b"frame neutral_lock_fail 1 1 1 ff080c12",
                b"call Surface.UnlockRect", b"call Device.Release")
    for marker in required:
        if marker not in old:
            raise SystemExit(f"backdrop fixture did not reach {marker!r}")
    print(f"PASS Win32: {len(old.splitlines())} trace lines, sha256={hashlib.sha256(old).hexdigest()}")


if __name__ == "__main__":
    main()
