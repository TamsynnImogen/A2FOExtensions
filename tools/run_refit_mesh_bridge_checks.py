#!/usr/bin/env python3
"""Build the mesh tap ABI fixture and run it with the guard in private Wine."""

import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import time

import run_refit_parts_checks as parts


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--guard-exe", type=Path,
                        default=ROOT / "build/refit_mesh_capture_guard_test.exe")
    parser.add_argument("--output-root", type=Path,
                        default=ROOT / "build/refit-mesh-bridge-checks")
    parser.add_argument("--xvfb", type=Path, default=parts.DEFAULT_XVFB)
    parser.add_argument("--wine", default="wine")
    parser.add_argument("--compiler", default="i686-w64-mingw32-g++")
    parser.add_argument("--compile-only", action="store_true")
    args = parser.parse_args()
    try:
        output_root = parts.private_output(args.output_root)
        if not args.guard_exe.is_file():
            raise ValueError(f"guard executable missing: {args.guard_exe}")
        if not args.compile_only and not args.xvfb.is_file():
            raise ValueError(f"private Xvfb missing: {args.xvfb}")
        if shutil.which(args.compiler) is None or (not args.compile_only and shutil.which(args.wine) is None):
            raise ValueError("compiler or Wine missing")
        output_root.mkdir(parents=True, exist_ok=True)
        run_dir = parts.private_output(output_root / time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
                                       / str(os.getpid()))
        run_dir.mkdir(parents=True, exist_ok=False)
        paths = {
            "abi_cpp": ROOT / "tests/refit_mesh_bridge_test.cpp",
            "abi_assembly": ROOT / "tests/refit_mesh_bridge_test.S",
            "bridge_assembly": ROOT / "core/refit_mesh_bridge.S",
            "guard_cpp": ROOT / "tests/refit_mesh_capture_guard_test.cpp",
            "capture_cpp": ROOT / "core/refit_mesh_capture.cpp",
            "capture_header": ROOT / "core/refit_mesh_capture.hpp",
            "profile_header": ROOT / "core/refit_scene_profile.hpp",
            "native_capture_header": ROOT / "core/native11_capture.hpp",
            "native_hooks_ffi": ROOT / "rust/native_hooks_ffi.h",
            "runner": Path(__file__).resolve(),
            "runner_helper": Path(parts.__file__).resolve(),
        }
        sources = {}
        snapshots = {}
        for name, path in paths.items():
            destination = run_dir / "inputs" / path.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            digest = parts.sha256(path)
            shutil.copyfile(path, destination)
            if parts.sha256(destination) != digest:
                raise RuntimeError(f"source snapshot mismatch: {name}")
            sources[name] = {"path": str(path), "sha256": digest,
                             "snapshot_path": str(destination)}
            snapshots[name] = destination
        executable = run_dir / "refit_mesh_bridge_test.exe"
        compile_cmd = [args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra",
                       "-Wpedantic", "-Werror", "-msse2", "-static", "-o", str(executable),
                       str(snapshots["abi_cpp"]), str(snapshots["abi_assembly"]),
                       str(snapshots["bridge_assembly"])]
        compile_result = parts.run_logged(compile_cmd, run_dir / "compile.log",
                                          os.environ.copy(), 120, ROOT)
        receipt = {"sources": sources,
                   "guard_executable": {"path": str(args.guard_exe.resolve()),
                                        "sha256": parts.sha256(args.guard_exe)},
                   "toolchain": {"compiler": parts.version(args.compiler),
                                 "wine": parts.version(args.wine)},
                   "compile": compile_result}
        if compile_result["exit_code"] != 0:
            receipt["status"] = "compile-failed"
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"FAIL: ABI fixture compile; see {run_dir / 'compile.log'}")
            return 1
        receipt["abi_executable"] = {"path": str(executable), "sha256": parts.sha256(executable)}
        if any(parts.sha256(path) != sources[name]["sha256"] for name, path in paths.items()):
            raise RuntimeError("source changed during compilation")
        if args.compile_only:
            receipt["status"] = "compiled-only"
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"PASS: ABI fixture compiled; evidence: {run_dir}")
            return 0
        work = run_dir / "private"
        work.mkdir()
        prefix = work / "prefix"
        runtime = work / "runtime"
        prefix.mkdir()
        runtime.mkdir()
        env = os.environ.copy()
        for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                    "SESSION_MANAGER", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP",
                    "KDE_FULL_SESSION", "QT_QPA_PLATFORM", "STEAM_COMPAT_DATA_PATH",
                    "LD_PRELOAD", "PULSE_SERVER", "PIPEWIRE_REMOTE"):
            env.pop(key, None)
        env.update(WINEPREFIX=str(prefix), WINEDEBUG="err+all", XDG_RUNTIME_DIR=str(runtime),
                   WINEDLLOVERRIDES="mmdevapi=;winepulse=;winealsa=;"
                                       "winemenubuilder.exe=d;winedbg.exe=d")
        xvfb, display_name = parts.start_xvfb(args.xvfb, work / "xvfb.log", env)
        env["DISPLAY"] = display_name
        try:
            init = parts.run_logged([args.wine, "wineboot", "-u"], work / "wineboot.log",
                                    env, 90, work)
            abi = guard = None
            if init["exit_code"] == 0:
                abi = parts.run_logged([args.wine, str(executable)], work / "abi.log", env, 60, work)
                guard = parts.run_logged([args.wine, str(args.guard_exe.resolve())],
                                         work / "guard.log", env, 60, work)
            receipt["private_run"] = {"display": display_name, "prefix": str(prefix),
                                      "wineboot": init, "abi": abi, "guard": guard}
        finally:
            parts.stop_process(xvfb)
        if parts.sha256(args.guard_exe) != receipt["guard_executable"]["sha256"]:
            raise RuntimeError("guard executable changed during run")
        receipt["status"] = "pass" if all(receipt["private_run"][key] and
                                           receipt["private_run"][key]["exit_code"] == 0
                                           for key in ("wineboot", "abi", "guard")) else "fail"
        (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print(f"{receipt['status'].upper()}: mesh bridge ABI and capture guard; evidence: {run_dir}")
        return 0 if receipt["status"] == "pass" else 1
    except (OSError, ValueError, RuntimeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
