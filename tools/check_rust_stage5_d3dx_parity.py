#!/usr/bin/env python3
"""Build and compare frozen/current D3DX bridge against Win32 PE/COM mocks.

The test uses scratch PE32 images and mock memory protection, atomic, BC, and
D3DX callbacks. It creates no D3D device, game process, window, or input event.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/rust-migration-stage5/d3dx-work"
BEFORE = ROOT / "build/rust-migration-stage5/before/renderer/d3d8_compat/game/d3dx_bridge.cpp"
CURRENT = ROOT / "renderer/d3d8_compat/game/d3dx_bridge.cpp"
FIXTURE = ROOT / "tests/rust_stage5_d3dx_parity.cpp"
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
        ('#include "factory.hpp"', '#include "mock_d3dx.hpp"'),
        ('#include "bc_catalog.hpp"', ''),
    ):
        if content.count(old) != 1:
            raise RuntimeError(f"unexpected include in {source}: {old}")
        content = content.replace(old, new, 1)
    if current:
        old = '#include "../../../rust/d3dx_ffi.h"'
        if content.count(old) != 1:
            raise RuntimeError(f"unexpected Rust include in {source}")
        content = content.replace(old, f'#include "{ROOT / "rust/d3dx_ffi.h"}"', 1)
    destination.write_text(content)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-lib", type=Path, default=ARCHIVE)
    parser.add_argument("--run-win32", action="store_true", help="run both mock EXEs headlessly under Wine")
    parser.add_argument("--wine-prefix", type=Path, default=Path("/tmp/a2fo-engine-compat-wine"))
    args = parser.parse_args()
    if not (WORK / "mock_d3dx.hpp").is_file():
        raise RuntimeError("missing isolated mock_d3dx.hpp")
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        raise RuntimeError(f"missing fresh integrated Rust archive: {archive}")
    before_copy = WORK / "before_d3dx.cpp"
    current_copy = WORK / "current_d3dx.cpp"
    copied_source(BEFORE, before_copy, False)
    copied_source(CURRENT, current_copy, True)
    flags = ["-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Werror", "-static", f"-I{WORK}"]
    before_exe = WORK / "before_d3dx_win32.exe"
    current_exe = WORK / "current_d3dx_win32.exe"
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
    old_trace = WORK / "before_d3dx_win32.trace"
    new_trace = WORK / "current_d3dx_win32.trace"
    run("wine", str(before_exe), env=env, output=old_trace)
    run("wine", str(current_exe), env=env, output=new_trace)
    old = old_trace.read_bytes()
    new = new_trace.read_bytes()
    if old != new:
        diff = difflib.unified_diff(old.decode(errors="replace").splitlines(),
                                    new.decode(errors="replace").splitlines(),
                                    fromfile=str(old_trace), tofile=str(new_trace), lineterm="")
        print("\n".join(list(diff)[:120]))
        raise SystemExit("D3DX parity mismatch")
    required = (b"install valid 1", b"create decoded_fallback 8876086a",
                b"create is_bc_throw 8007000e", b"install install_logger_throw 0",
                b"call VirtualProtect", b"call InterlockedExchangePointer")
    for marker in required:
        if marker not in old:
            raise SystemExit(f"D3DX fixture did not reach {marker!r}")
    print(f"PASS Win32: {len(old.splitlines())} trace lines, sha256={hashlib.sha256(old).hexdigest()}")


if __name__ == "__main__":
    main()
