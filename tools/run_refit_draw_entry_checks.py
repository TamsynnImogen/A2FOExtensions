#!/usr/bin/env python3
"""Compile and run four D3D8 draw ABI probes in private Wine/Xvfb sessions."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import run_refit_parts_checks as parts


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--display-dll", type=Path,
                        default=ROOT / "build/refit/draw-entry/A2FORefitDisplay.dll")
    parser.add_argument("--output-root", type=Path,
                        default=ROOT / "build/refit-draw-entry-checks")
    parser.add_argument("--backend", choices=("vulkan", "d3d12", "both"), default="both")
    parser.add_argument("--xvfb", type=Path, default=parts.DEFAULT_XVFB)
    parser.add_argument("--icd", type=Path, default=parts.DEFAULT_ICD)
    parser.add_argument("--wine", default="wine")
    parser.add_argument("--compiler", default="i686-w64-mingw32-g++")
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--reuse-prefix-vulkan", type=Path)
    parser.add_argument("--reuse-prefix-d3d12", type=Path)
    parser.add_argument("--display-base", type=int,
                        help="dedicated private Xvfb display number; both uses this and the next")
    args = parser.parse_args()
    try:
        output_root = parts.private_output(args.output_root)
        if not args.display_dll.is_file():
            raise ValueError(f"Display DLL missing: {args.display_dll}")
        if not args.compile_only and (not args.xvfb.is_file() or not args.icd.is_file()):
            raise ValueError("private Xvfb or Vulkan ICD missing")
        if shutil.which(args.compiler) is None or (not args.compile_only and shutil.which(args.wine) is None):
            raise ValueError("compiler or Wine executable missing")
        output_root.mkdir(parents=True, exist_ok=True)
        run_dir = parts.private_output(output_root / time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
                                       / str(os.getpid()))
        run_dir.mkdir(parents=True, exist_ok=False)
        paths = {
            "fixture": ROOT / "tests/refit_draw_entry_abi_test.cpp",
            "assembly": ROOT / "tests/refit_draw_entry_abi_test.S",
            "host_header": ROOT / "renderer/refit/host.h",
            "runner": Path(__file__).resolve(),
            "runner_helper": Path(parts.__file__).resolve(),
        }
        sources = {}
        snapshots = {}
        for name, path in paths.items():
            relative = path.relative_to(ROOT)
            destination = run_dir / "inputs" / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            digest = parts.sha256(path)
            shutil.copyfile(path, destination)
            if parts.sha256(destination) != digest:
                raise RuntimeError(f"source snapshot changed: {name}")
            sources[name] = {"path": str(path), "sha256": digest,
                             "snapshot_path": str(destination)}
            snapshots[name] = destination
        executable = run_dir / "refit_draw_entry_abi_test.exe"
        compile_cmd = [args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-msse2", str(snapshots["fixture"]), str(snapshots["assembly"]),
                       "-o", str(executable), "-static", "-static-libgcc", "-static-libstdc++",
                       "-luser32", "-lkernel32"]
        compile_result = parts.run_logged(compile_cmd, run_dir / "compile.log", os.environ.copy(),
                                          120, ROOT)
        receipt = {"sources": sources,
                   "toolchain": {"compiler": parts.version(args.compiler),
                                 "wine": parts.version(args.wine)},
                   "display": {"path": str(args.display_dll.resolve()),
                               "sha256": parts.sha256(args.display_dll)},
                   "compile": compile_result, "backends": {}}
        if compile_result["exit_code"] != 0:
            receipt["status"] = "compile-failed"
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"FAIL: compile; see {run_dir / 'compile.log'}")
            return 1
        receipt["fixture"] = {"path": str(executable), "sha256": parts.sha256(executable)}
        if any(parts.sha256(path) != sources[name]["sha256"] for name, path in paths.items()):
            raise RuntimeError("source changed during fixture compilation")
        if args.compile_only:
            receipt["status"] = "compiled-only"
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"PASS: fixture compiled; evidence: {run_dir}")
            return 0
        backends = ("vulkan", "d3d12") if args.backend == "both" else (args.backend,)
        for index, backend in enumerate(backends):
            work = run_dir / backend
            work.mkdir()
            display = work / "A2FORefitDisplay.dll"
            shutil.copyfile(args.display_dll, display)
            if parts.sha256(display) != receipt["display"]["sha256"]:
                raise RuntimeError("private Display DLL hash mismatch")
            supplied = args.reuse_prefix_vulkan if backend == "vulkan" else args.reuse_prefix_d3d12
            prefix = parts.private_output(supplied) if supplied else work / "prefix"
            runtime = work / "runtime"
            if supplied:
                if not prefix.is_dir():
                    raise ValueError(f"initialized private prefix missing: {prefix}")
            else:
                prefix.mkdir()
            runtime.mkdir()
            env = os.environ.copy()
            for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                        "SESSION_MANAGER", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP",
                        "KDE_FULL_SESSION", "QT_QPA_PLATFORM", "STEAM_COMPAT_DATA_PATH",
                        "LD_PRELOAD", "PULSE_SERVER", "PIPEWIRE_REMOTE"):
                env.pop(key, None)
            env.update(WINEPREFIX=str(prefix), WINEDEBUG="err+all", XDG_RUNTIME_DIR=str(runtime),
                       VK_ICD_FILENAMES=str(args.icd.resolve()), LIBGL_ALWAYS_SOFTWARE="1",
                       WINEDLLOVERRIDES="d3d8=;d3d9=;mmdevapi=;winepulse=;winealsa=;"
                                           "winemenubuilder.exe=d;winedbg.exe=d")
            if args.display_base is None:
                xvfb, display_name = parts.start_xvfb(args.xvfb, work / "xvfb.log", env)
            else:
                number = args.display_base + index
                if not 1 <= number <= 65535 or \
                   Path(f"/tmp/.X{number}-lock").exists() or \
                   Path(f"/tmp/.X11-unix/X{number}").exists():
                    raise ValueError(f"private Xvfb display :{number} is unavailable")
                display_name = f":{number}"
                with (work / "xvfb.log").open("wb") as log:
                    xvfb = subprocess.Popen([str(args.xvfb), display_name, "-screen", "0",
                                             "640x480x24", "-nolisten", "tcp", "-noreset"],
                                            env=env, stdin=subprocess.DEVNULL, stdout=log,
                                            stderr=subprocess.STDOUT, start_new_session=True)
                time.sleep(1)
                if xvfb.poll() is not None:
                    raise RuntimeError(f"private Xvfb failed; see {work / 'xvfb.log'}")
            env["DISPLAY"] = display_name
            try:
                init = None if supplied else parts.run_logged([args.wine, "wineboot", "-u"],
                                                               work / "wineboot.log", env, 90, work)
                check = None
                if supplied or init["exit_code"] == 0:
                    check = parts.run_logged([args.wine, str(executable), backend, str(display)],
                                             work / "fixture.log", env, 180, work)
                receipt["backends"][backend] = {"display": display_name,
                    "prefix": str(prefix), "reused_private_prefix": bool(supplied),
                    "vulkan_icd": {"path": str(args.icd.resolve()), "sha256": parts.sha256(args.icd)},
                    "wineboot": init, "fixture": check}
            finally:
                parts.stop_process(xvfb)
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        receipt["status"] = "pass" if all(v["fixture"] and v["fixture"]["exit_code"] == 0
                                             for v in receipt["backends"].values()) else "fail"
        (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print(f"{receipt['status'].upper()}: four draw entry ABI checks; evidence: {run_dir}")
        return 0 if receipt["status"] == "pass" else 1
    except (OSError, ValueError, RuntimeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
