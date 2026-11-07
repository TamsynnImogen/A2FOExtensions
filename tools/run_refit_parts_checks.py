#!/usr/bin/env python3
"""Build and run the Refit parts fixture in a private Wine/Xvfb session.

The two native backends run in separate processes and prefixes. All generated
files stay under build/; this runner never opens the game or its installation.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import signal
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_XVFB = Path("/home/tamsynn/Applications/iStripper/tools/usr/bin/Xvfb")
# The tested system Wine uses new WoW64, whose Unix Vulkan loader is x86_64.
DEFAULT_ICD = Path("/usr/share/vulkan/icd.d/lvp_icd.x86_64.json")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def version(command):
    try:
        result = subprocess.run([command, "--version"], stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=10, check=False)
        return {"command": command, "exit_code": result.returncode,
                "first_line": result.stdout.splitlines()[0] if result.stdout else ""}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"command": command, "error": str(error)}


def private_output(path):
    path = path.resolve()
    if not path.is_relative_to((ROOT / "build").resolve()):
        raise ValueError("output must be beneath this checkout's build directory")
    if any(parent.is_symlink() for parent in (path, *path.parents) if parent != ROOT.parent):
        raise ValueError("output path contains a symlink")
    return path


def run_logged(argv, log, env, timeout, cwd):
    started = time.monotonic()
    with log.open("wb") as output:
        process = subprocess.Popen(argv, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        timed_out = False
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=5)
            code = 124
    return {"argv": [str(arg) for arg in argv], "exit_code": code,
            "timed_out": timed_out, "elapsed_seconds": round(time.monotonic() - started, 3),
            "log": str(log)}


def start_xvfb(binary, log, env):
    with log.open("wb") as output:
        process = subprocess.Popen([str(binary), "-displayfd", "1", "-screen", "0",
                                    "640x480x24", "-nolisten", "tcp", "-noreset"],
                                   env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=output, start_new_session=True)
    ready, _, _ = select.select([process.stdout], [], [], 15)
    line = process.stdout.readline().strip() if ready else b""
    if not line.isdigit() or process.poll() is not None:
        stop_process(process)
        raise RuntimeError(f"private Xvfb did not allocate a display; see {log}")
    return process, ":" + line.decode("ascii")


def stop_process(process):
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=5)
    if process.stdout:
        process.stdout.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--display-dll", type=Path, default=ROOT / "build/refit/scoped-parts-api/A2FORefitDisplay.dll")
    parser.add_argument("--renderer-dll", type=Path, default=ROOT / "build/refit/scoped-parts-api/A2FORefitRenderer.dll")
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/refit-parts-checks")
    parser.add_argument("--backend", choices=("vulkan", "d3d12", "both"), default="both")
    parser.add_argument("--xvfb", type=Path, default=DEFAULT_XVFB)
    parser.add_argument("--icd", type=Path, default=DEFAULT_ICD,
                        help="Vulkan ICD JSON (defaults to x86_64 Lavapipe for tested new-WoW64 Wine;"
                             " pass an i686 ICD explicitly for traditional 32-bit Wine)")
    parser.add_argument("--wine", default="wine")
    parser.add_argument("--compiler", default="i686-w64-mingw32-g++")
    parser.add_argument("--compile-only", action="store_true")
    args = parser.parse_args()

    try:
        output_root = private_output(args.output_root)
        for name, path in (("display", args.display_dll), ("renderer", args.renderer_dll)):
            if not path.is_file():
                raise ValueError(f"{name} DLL missing: {path}")
        if not args.compile_only and not args.xvfb.is_file():
            raise ValueError(f"private Xvfb missing: {args.xvfb}")
        if not args.compile_only and not args.icd.is_file():
            raise ValueError(f"Vulkan ICD missing: {args.icd}")
        if shutil.which(args.compiler) is None or (not args.compile_only and shutil.which(args.wine) is None):
            raise ValueError("required compiler or Wine executable missing")
        output_root.mkdir(parents=True, exist_ok=True)
        run_dir = private_output(output_root / time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()) / str(os.getpid()))
        run_dir.mkdir(parents=True, exist_ok=False)
        executable = run_dir / "refit_parts_runtime_test.exe"
        source_paths = {
            "fixture": ROOT / "tests/refit_parts_runtime_test.cpp",
            "host_header": ROOT / "renderer/refit/host.h",
            "synthetic_sod_header": ROOT / "tests/native11_model_fixture.hpp",
            "runner": Path(__file__).resolve(),
        }
        source_hashes = {name: {"path": str(path), "sha256": sha256(path)}
                         for name, path in source_paths.items()}
        input_paths = {
            "fixture": run_dir / "inputs/tests/refit_parts_runtime_test.cpp",
            "host_header": run_dir / "inputs/renderer/refit/host.h",
            "synthetic_sod_header": run_dir / "inputs/tests/native11_model_fixture.hpp",
            "runner": run_dir / "inputs/tools/run_refit_parts_checks.py",
        }
        for name, destination in input_paths.items():
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source_paths[name], destination)
            if sha256(destination) != source_hashes[name]["sha256"]:
                raise RuntimeError(f"source snapshot hash mismatch: {name}")
            source_hashes[name]["snapshot_path"] = str(destination)
        compile_cmd = [args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                       str(input_paths["fixture"]), "-o", str(executable),
                       "-static", "-static-libgcc", "-static-libstdc++",
                       "-luser32", "-lkernel32"]
        compile_result = run_logged(compile_cmd, run_dir / "compile.log", os.environ.copy(), 120, ROOT)
        receipt = {"fixture": str(executable),
                   "sources": source_hashes,
                   "toolchain": {"compiler": version(args.compiler),
                                 "wine": version(args.wine)}, "inputs": {
            "display": {"path": str(args.display_dll.resolve()), "sha256": sha256(args.display_dll)},
            "renderer": {"path": str(args.renderer_dll.resolve()), "sha256": sha256(args.renderer_dll)}},
            "compile": compile_result, "backends": {}}
        if compile_result["exit_code"] != 0:
            receipt["status"] = "compile-failed"
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"FAIL: fixture compile; see {run_dir / 'compile.log'}")
            return 1
        receipt["fixture_sha256"] = sha256(executable)
        if any(sha256(path) != source_hashes[name]["sha256"] for name, path in source_paths.items()) or \
           any(sha256(path) != source_hashes[name]["sha256"] for name, path in input_paths.items()):
            raise RuntimeError("fixture source changed during compilation")
        if args.compile_only:
            receipt["status"] = "compiled-only"
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            print(f"PASS: fixture compiled; GPU checks not run; evidence: {run_dir}")
            return 0

        backends = ("vulkan", "d3d12") if args.backend == "both" else (args.backend,)
        for backend in backends:
            work = run_dir / backend
            work.mkdir()
            display = work / "A2FORefitDisplay.dll"
            renderer = work / "A2FORefitRenderer.dll"
            shutil.copyfile(args.display_dll, display)
            shutil.copyfile(args.renderer_dll, renderer)
            if sha256(display) != receipt["inputs"]["display"]["sha256"] or sha256(renderer) != receipt["inputs"]["renderer"]["sha256"]:
                raise RuntimeError("private DLL staging hash mismatch")
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
                       VK_ICD_FILENAMES=str(args.icd.resolve()),
                       WINEDLLOVERRIDES="d3d8=;d3d9=;mmdevapi=;winepulse=;winealsa=;"
                                           "winemenubuilder.exe=d;winedbg.exe=d")
            xvfb, display_name = start_xvfb(args.xvfb, work / "xvfb.log", env)
            env["DISPLAY"] = display_name
            try:
                init = run_logged([args.wine, "wineboot", "-u"], work / "wineboot.log", env, 90, work)
                check = None
                if init["exit_code"] == 0:
                    fixture_args = [args.wine, str(executable), backend, str(display),
                                    str(renderer), str(work)]
                    check = run_logged(fixture_args, work / "fixture.log", env, 180, work)
                receipt["backends"][backend] = {"display": display_name, "prefix": str(prefix),
                                                  "vulkan_icd": {"path": str(args.icd.resolve()),
                                                                 "sha256": sha256(args.icd)},
                                                  "wineboot": init, "fixture": check}
            finally:
                stop_process(xvfb)
            (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        receipt["status"] = "pass" if all(v["fixture"] and v["fixture"]["exit_code"] == 0
                                              for v in receipt["backends"].values()) else "fail"
        (run_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print(f"{receipt['status'].upper()}: Refit parts GPU checks; evidence: {run_dir}")
        return 0 if receipt["status"] == "pass" else 1
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
