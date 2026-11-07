#!/usr/bin/env python3
"""Run isolated Refit window handoff fixtures on private software GPU surfaces."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from run_refit_parts_checks import (DEFAULT_ICD, DEFAULT_XVFB, private_output,
                                    run_logged, sha256, start_xvfb, stop_process, version)

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/refit-window-checks")
    parser.add_argument("--display-dll", type=Path, default=ROOT / "build/refit/window-handoff-guarded/target/display/i686-pc-windows-msvc/release/display.dll")
    parser.add_argument("--renderer-dll", type=Path, default=ROOT / "build/refit/scoped-parts-api/A2FORefitRenderer.dll")
    parser.add_argument("--xvfb", type=Path, default=DEFAULT_XVFB)
    parser.add_argument("--icd", type=Path, default=DEFAULT_ICD)
    parser.add_argument("--compiler", default="i686-w64-mingw32-g++")
    parser.add_argument("--wine", default="wine")
    args = parser.parse_args()
    run_dir = None
    receipt = {}
    try:
        output = private_output(args.output_root)
        for path in (args.display_dll, args.renderer_dll, args.xvfb, args.icd):
            if not path.is_file():
                raise ValueError(f"required input missing: {path}")
        if not shutil.which(args.compiler) or not shutil.which(args.wine):
            raise ValueError("compiler or Wine executable missing")
        output.mkdir(parents=True, exist_ok=True)
        run_dir = output / f"{time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())}-{os.getpid()}-{time.time_ns()}"
        run_dir.mkdir()
        source = ROOT / "tests/refit_window_runtime_test.cpp"
        runner = Path(__file__).resolve()
        fixture = run_dir / "refit_window_runtime_test.exe"
        compile_command = [args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                           "-Werror", "-static", "-static-libgcc", "-static-libstdc++",
                           str(source), "-o", str(fixture), "-luser32", "-lkernel32"]
        receipt = {"sources": {str(p): sha256(p) for p in (source, runner, ROOT / "renderer/refit/host.h",
                                                         ROOT / "tests/native11_model_fixture.hpp")},
                   "inputs": {str(p): sha256(p) for p in (args.display_dll, args.renderer_dll)},
                   "toolchain": {"compiler": version(args.compiler), "wine": version(args.wine)},
                   "compile": run_logged(compile_command, run_dir / "compile.log", os.environ.copy(), 120, ROOT),
                   "cases": {}}
        if receipt["compile"]["exit_code"] != 0:
            receipt["status"] = "compile-failed"
            return 1
        receipt["fixture"] = {"path": str(fixture), "sha256": sha256(fixture)}
        for backend in ("vulkan", "d3d12"):
            work = run_dir / backend
            work.mkdir()
            display = work / "A2FORefitDisplay.dll"
            renderer = work / "A2FORefitRenderer.dll"
            shutil.copyfile(args.display_dll, display)
            shutil.copyfile(args.renderer_dll, renderer)
            if sha256(display) != receipt["inputs"][str(args.display_dll)] or sha256(renderer) != receipt["inputs"][str(args.renderer_dll)]:
                raise RuntimeError("staged DLL hash mismatch")
            prefix, runtime = work / "prefix", work / "runtime"
            prefix.mkdir(); runtime.mkdir(); runtime.chmod(0o700)
            env = os.environ.copy()
            for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                        "SESSION_MANAGER", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP",
                        "KDE_FULL_SESSION", "QT_QPA_PLATFORM", "STEAM_COMPAT_DATA_PATH",
                        "LD_PRELOAD", "PULSE_SERVER", "PIPEWIRE_REMOTE"):
                env.pop(key, None)
            env.update(WINEPREFIX=str(prefix), XDG_RUNTIME_DIR=str(runtime), WINEDEBUG="err+all",
                       VK_ICD_FILENAMES=str(args.icd.resolve()), LIBGL_ALWAYS_SOFTWARE="1",
                       GALLIUM_DRIVER="llvmpipe",
                       WINEDLLOVERRIDES="d3d8=;d3d9=;mmdevapi=;winepulse=;winealsa=;winemenubuilder.exe=d;winedbg.exe=d")
            xvfb, display_name = start_xvfb(args.xvfb, work / "xvfb.log", env)
            env["DISPLAY"] = display_name
            try:
                boot = run_logged([args.wine, "wineboot", "-u"], work / "wineboot.log", env, 90, work)
                receipt["cases"][backend] = {"wineboot": boot}
                if boot["exit_code"] != 0:
                    receipt["status"] = "wineboot-failed"
                    return 1
                for mode in ("fixed", "movable"):
                    result = run_logged([args.wine, str(fixture), mode, backend, str(display), str(renderer)],
                                        work / f"{mode}.log", env, 120, work)
                    receipt["cases"][backend][mode] = result
                    if result["exit_code"] != 0:
                        receipt["status"] = "fixture-failed"
                        return 1
            finally:
                stop_process(xvfb)
        if any(sha256(p) != h for p, h in ((Path(k), v) for k, v in receipt["sources"].items())):
            raise RuntimeError("source changed during checks")
        receipt["status"] = "pass"
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["status"] = "runner-error"
        receipt["error"] = str(error)
        return 1
    finally:
        if run_dir is not None:
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"{receipt.get('status', 'unknown').upper()}: Refit window checks; evidence: {run_dir}")
        elif receipt.get("error"):
            print(f"FAIL: {receipt['error']}", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
