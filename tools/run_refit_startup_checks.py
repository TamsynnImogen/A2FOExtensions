#!/usr/bin/env python3
"""Run the CPU-only Refit startup policy fixture in a private Wine/Xvfb session."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from run_refit_host_checks import (
    DEFAULT_XVFB, private_output, run_logged, sha256, start_xvfb,
    stop_process, version,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    "tests/refit_startup_policy_test.cpp",
    "renderer/refit/startup.cpp",
    "renderer/refit/startup.hpp",
    "renderer/refit/fo_profile.hpp",
    "renderer/refit/host.h",
    "renderer/refit/workspace_bridge.S",
    "core/refit_scene_profile.hpp",
    "rust/native_hooks_ffi.h",
    "core/extension_roots.cpp",
    "core/extension_roots.hpp",
    "rust/roots_ffi.h",
    "tools/run_refit_startup_checks.py",
    "tools/run_refit_host_checks.py",
    "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path,
                        default=ROOT / "build/refit-startup-checks")
    parser.add_argument("--compiler", default="i686-w64-mingw32-g++")
    parser.add_argument("--wine", default="wine")
    parser.add_argument("--xvfb", type=Path, default=DEFAULT_XVFB)
    args = parser.parse_args()
    receipt = {}
    run_dir = None
    try:
        output = private_output(args.output_root)
        for rel in SOURCES:
            if not (ROOT / rel).is_file():
                raise ValueError(f"required source or Rust library missing: {rel}")
        if not args.xvfb.is_file():
            raise ValueError(f"private Xvfb missing: {args.xvfb}")
        if shutil.which(args.compiler) is None or shutil.which(args.wine) is None:
            raise ValueError("required compiler or Wine executable missing")
        output.mkdir(parents=True, exist_ok=True)
        run_dir = private_output(output / f"{time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())}-{os.getpid()}-{time.time_ns()}")
        run_dir.mkdir(parents=True, exist_ok=False)
        inputs = run_dir / "inputs"
        source_hashes = {}
        for rel in SOURCES:
            source = ROOT / rel
            target = inputs / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            digest = sha256(source)
            if sha256(target) != digest:
                raise RuntimeError(f"source snapshot hash mismatch: {rel}")
            source_hashes[rel] = {"path": str(source), "sha256": digest,
                                  "snapshot": str(target)}
        executable = run_dir / "refit_startup_policy_test.exe"
        command = [args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra",
                   "-Wpedantic", "-Werror", "-static", "-static-libgcc",
                   "-static-libstdc++", str(inputs / "tests/refit_startup_policy_test.cpp"),
                   str(inputs / "renderer/refit/startup.cpp"),
                   str(inputs / "core/extension_roots.cpp"),
                   str(inputs / "renderer/refit/workspace_bridge.S"),
                   str(inputs / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"),
                   "-o", str(executable), "-lshell32", "-luser32", "-lkernel32"]
        compiler_path = Path(shutil.which(args.compiler)).resolve()
        wine_path = Path(shutil.which(args.wine)).resolve()
        receipt = {"sources": source_hashes,
                   "toolchain": {"compiler": {**version(args.compiler),
                                              "path": str(compiler_path),
                                              "sha256": sha256(compiler_path)},
                                 "wine": {**version(args.wine),
                                          "path": str(wine_path),
                                          "sha256": sha256(wine_path)},
                                 "xvfb": {"path": str(args.xvfb.resolve()),
                                          "sha256": sha256(args.xvfb)}},
                   "compile": run_logged(command, run_dir / "compile.log",
                                         os.environ.copy(), 120, ROOT), "cases": {}}
        if receipt["compile"]["exit_code"] != 0:
            receipt["status"] = "compile-failed"
            return 1
        receipt["executable"] = {"path": str(executable), "sha256": sha256(executable)}
        for rel, prior in source_hashes.items():
            if sha256(ROOT / rel) != prior["sha256"] or sha256(inputs / rel) != prior["sha256"]:
                raise RuntimeError(f"input changed during compile: {rel}")

        env = os.environ.copy()
        for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                    "SESSION_MANAGER", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP",
                    "KDE_FULL_SESSION", "QT_QPA_PLATFORM", "STEAM_COMPAT_DATA_PATH",
                    "LD_PRELOAD", "PULSE_SERVER", "PIPEWIRE_REMOTE"):
            env.pop(key, None)
        prefix = run_dir / f"a2fo-refit-startup-policy-prefix-{os.getpid()}"
        runtime = run_dir / "runtime"
        prefix.mkdir()
        runtime.mkdir()
        env.update(WINEPREFIX=str(prefix), XDG_RUNTIME_DIR=str(runtime),
                   WINEDEBUG="err+all",
                   WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d")
        xvfb, display = start_xvfb(args.xvfb, run_dir / "xvfb.log", env)
        env["DISPLAY"] = display
        receipt["runtime"] = {"prefix": str(prefix), "display": display,
                              "xvfb_log": str(run_dir / "xvfb.log")}
        try:
            boot = run_logged([args.wine, "wineboot", "-u"],
                              run_dir / "wineboot.log", env, 90, run_dir)
            receipt["wineboot"] = boot
            if boot["exit_code"] != 0:
                receipt["status"] = "wineboot-failed"
                return 1
            for name, switch in (("normal", None), ("slash_d3d9", "/d3d9"),
                                 ("dash_d3d9", "-d3d9")):
                argv = [args.wine, str(executable), "/mod", "Refit Contract Child"]
                if switch:
                    argv.append(switch)
                result = run_logged(argv, run_dir / f"{name}.log", env, 90, run_dir)
                receipt["cases"][name] = result
                if result["exit_code"] != 0:
                    receipt["status"] = "fixture-failed"
                    return 1
        finally:
            stop_process(xvfb)
        receipt["status"] = "pass"
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["status"] = "runner-error"
        receipt["error"] = str(error)
        return 1
    finally:
        if run_dir is not None:
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"{receipt.get('status', 'unknown').upper()}: startup policy checks; evidence: {run_dir}")
        elif receipt.get("error"):
            print(f"FAIL: {receipt['error']}", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
