#!/usr/bin/env python3
"""Run held Rust migration build and native regression phases sequentially."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
WINDOWS_LIB = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
HOST_LIB = ROOT / "build/rust/release/liba2fo_core.a"
RUST_INPUT_NAMES = {"Cargo.toml", "Cargo.lock"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".inl",
                   ".s", ".S", ".asm", ".def", ".rc"}
TEST_DATA_SUFFIXES = {".json", ".toml", ".yaml", ".yml", ".ini", ".cfg",
                      ".conf", ".dat", ".txt"}
DRIVERS = (
    "tools/run_rust_stage7_parity.py",
    "tools/run_rust_stage8_resume_checks.py",
    "tools/run_builtin_stage10_tests.py",
    "tools/run_rust_stage9_regressions.py",
)
NATIVE_TIMEOUT = 5400
BUILD_TIMEOUT = 5400
PROCESS_TERM_GRACE = 5
NATIVE_EXTRA_BUILD_TARGETS = (
    "$(BUILD_DIR)/engine_profiles_native_test.exe",
    "$(BUILD_DIR)/roots_dll_load_smoke.exe",
    "$(BUILD_DIR)/refit_loading_native.exe",
)
MAKE_TARGET_PATTERN = re.compile(r"\$\(BUILD_DIR\)/[A-Za-z0-9_./-]+\Z")


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def output_contains(path, output_dir):
    try:
        path.relative_to(output_dir)
        return True
    except ValueError:
        return False


def pin_paths(output_dir):
    paths = {ROOT / "Makefile", WINDOWS_LIB, HOST_LIB}
    paths.update((ROOT / "ArmadaL.exe", ROOT / "FleetOpsHook.dll"))
    paths.update((ROOT / "build/engine-compat-20261023/inputs/oct23/ArmadaL.exe",
                  ROOT / "build/engine-compat-20261023/inputs/oct23/FleetOpsHook.dll"))
    for directory in ("core", "modules", "sdk", "tests", "tools", "renderer",
                      "missions", "scripts"):
        base = ROOT / directory
        for path in base.rglob("*"):
            if (not path.is_file() or "__pycache__" in path.parts or
                    output_contains(path, output_dir)):
                continue
            if path.suffix in SOURCE_SUFFIXES or (
                    directory in ("tools", "tests") and path.suffix == ".py") or (
                    directory == "tests" and path.suffix in TEST_DATA_SUFFIXES):
                paths.add(path)
    rust_root = ROOT / "rust"
    for path in rust_root.rglob("*"):
        if not path.is_file() or "target" in path.parts or output_contains(path, output_dir):
            continue
        if path.suffix == ".rs" or path.name.endswith("_ffi.h") or path.name in RUST_INPUT_NAMES:
            paths.add(path)
    paths.update(ROOT / relative for relative in DRIVERS)
    return paths


def native_artifact_paths():
    paths = set()
    build = ROOT / "build"
    if build.exists():
        paths.update(path for path in build.iterdir()
                     if path.is_file() and path.suffix.lower() in {".exe", ".dll"})
    modules = build / "modules"
    if modules.exists():
        paths.update(path for path in modules.rglob("*.dll") if path.is_file())
    fixtures = build / "fixtures"
    if fixtures.exists():
        for path in fixtures.rglob("*"):
            if path.is_file():
                with path.open("rb") as stream:
                    if b"\0" in stream.read(8192):
                        paths.add(path)
    video_fixtures = build / "video-fixtures"
    if video_fixtures.exists():
        paths.update(path for path in video_fixtures.rglob("*") if path.is_file())
    return paths


def snapshot(output_dir, include_native_artifacts=False):
    paths = pin_paths(output_dir)
    if include_native_artifacts:
        paths.update(native_artifact_paths())
    missing = sorted(str(path) for path in paths if not path.is_file())
    if missing:
        raise FileNotFoundError("missing pinned inputs: " + ", ".join(missing))
    return {str(path.relative_to(ROOT)): sha256(path) for path in sorted(paths)}


def drift(before, after):
    return sorted(key for key in before.keys() | after.keys()
                  if before.get(key) != after.get(key))


def assert_archives_fresh():
    rust_inputs = [path for path in (ROOT / "rust").rglob("*")
                   if path.is_file() and "target" not in path.parts and
                   (path.suffix == ".rs" or path.name.endswith("_ffi.h") or
                    path.name in RUST_INPUT_NAMES)]
    newest = max(rust_inputs, key=lambda path: path.stat().st_mtime_ns)
    for archive in (WINDOWS_LIB, HOST_LIB):
        if not archive.is_file():
            raise FileNotFoundError(f"missing Rust archive: {archive}")
        if newest.stat().st_mtime_ns > archive.stat().st_mtime_ns:
            raise RuntimeError(f"Rust input {newest} is newer than {archive}; rebuild archives")


def write_json(path, data):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def terminate_process_group(process):
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    deadline = time.monotonic() + PROCESS_TERM_GRACE
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining > 0:
            time.sleep(min(0.1, remaining))
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def run_step(name, command, output, receipt, timeout, env, output_dir,
             native=False):
    before = snapshot(output_dir, include_native_artifacts=native)
    stdout_path = output / f"{name}.log"
    started = time.monotonic()
    code = None
    error = None
    try:
        assert_archives_fresh()
    except Exception as freshness_error:
        after = snapshot(output_dir, include_native_artifacts=native)
        row = {"name": name, "command": command, "exit_code": None,
               "elapsed_seconds": 0.0, "timeout_seconds": timeout,
               "log": str(stdout_path), "pins_before": before,
               "pins_after": after, "changed_pins": drift(before, after),
               "status": "failed", "error": str(freshness_error)}
        stdout_path.write_text(f"PRECONDITION FAILED: {freshness_error}\n")
        receipt["steps"].append(row)
        write_json(output / "results.json", receipt)
        raise RuntimeError(f"{name} precondition failed: {freshness_error}")
    with stdout_path.open("wb") as log:
        try:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT,
                                       start_new_session=True)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired as timeout_error:
                code = 124
                error = f"timed out after {timeout_error.timeout} seconds"
                terminate_process_group(process)
                log.write((str(timeout_error) + "\n").encode())
        except OSError as process_error:
            code = 127
            error = str(process_error)
            log.write((str(process_error) + "\n").encode())
    after = snapshot(output_dir, include_native_artifacts=native)
    changed = drift(before, after)
    try:
        assert_archives_fresh()
    except Exception as freshness_error:
        error = str(freshness_error)
    row = {
        "name": name,
        "command": command,
        "exit_code": code,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "timeout_seconds": timeout,
        "log": str(stdout_path),
        "pins_before": before,
        "pins_after": after,
        "changed_pins": changed,
        "status": "passed" if code == 0 and not changed else "failed",
    }
    if error:
        row["status"] = "failed"
    if error:
        row["error"] = error
    receipt["steps"].append(row)
    write_json(output / "results.json", receipt)
    print(f"{name}: {row['status']} ({row['elapsed_seconds']}s)", flush=True)
    if row["status"] != "passed":
        raise RuntimeError(f"{name} failed; see {stdout_path}" +
                           (f"; changed pins: {changed}" if changed else ""))


def build_steps():
    return [
        ("make-test", ["make", "-j2", "test", "CARGO=false"]),
        ("builtin-stage10-build", ["make", "-j2", "builtin-stage10-build",
                                    "CARGO=false"]),
        ("builtin-native-prerequisites", native_prerequisite_build_command()),
    ]


def native_prerequisite_build_command():
    makefile = (ROOT / "Makefile").read_text().splitlines()
    logical_lines = []
    current = ""
    for line in makefile:
        current += line.rstrip()
        if current.endswith("\\"):
            current = current[:-1] + " "
            continue
        logical_lines.append(current)
        current = ""
    if current:
        raise RuntimeError("unterminated Makefile continuation")

    targets = []
    found = False
    for line in logical_lines:
        if ":" not in line:
            continue
        lhs, rhs = line.split(":", 1)
        if lhs.strip() != "builtin-native-tests":
            continue
        found = True
        for token in rhs.split():
            if not MAKE_TARGET_PATTERN.fullmatch(token):
                raise RuntimeError(
                    f"unsupported builtin-native-tests prerequisite: {token!r}")
            targets.append(token)
    if not found:
        raise RuntimeError("Makefile has no builtin-native-tests prerequisite rule")
    targets.extend(NATIVE_EXTRA_BUILD_TARGETS)
    expanded = list(dict.fromkeys(token.replace("$(BUILD_DIR)", "build")
                                  for token in targets))
    return ["make", "-j2", *expanded, "CARGO=false"]


def native_steps(prefix, output):
    return [
        ("stage7-parity", ["python3", "tools/run_rust_stage7_parity.py",
                            "--wine-prefix", str(prefix), "--receipt",
                            str(output / "stage7-parity.json")]),
        ("stage8-regressions", ["python3", "tools/run_rust_stage8_resume_checks.py",
                                "--wine-prefix", str(prefix), "--output-dir",
                                str(output / "stage8-regressions")]),
        ("builtin-native-tests", ["make", "builtin-native-tests",
                                  f"A2FO_BUILTIN_WINEPREFIX={prefix}", "CARGO=false"]),
        ("stage10-consolidation", ["python3", "tools/run_builtin_stage10_tests.py",
                                   "--wine-prefix", str(prefix), "--output-dir",
                                   str(output / "consolidation")]),
    ]


def wine_environment(prefix):
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(WINEPREFIX=str(prefix), A2FO_BUILTIN_WINEPREFIX=str(prefix),
               A2FO_RUST_WINEPREFIX=str(prefix), WINEDEBUG="-all",
               LIBGL_ALWAYS_SOFTWARE="1",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    return env


def start_keepalive(prefix, output, env):
    if not (prefix / "system.reg").is_file():
        raise RuntimeError(f"an initialized private Wine prefix is required: {prefix}")
    if not (prefix / "drive_c").is_dir():
        raise RuntimeError(f"private Wine prefix has no drive_c directory: {prefix}")
    source = output / "keepalive.cpp"
    executable = output / "keepalive.exe"
    log_path = output / "keepalive.log"
    source.write_text(
        "#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include <cstdio>\n"
        "int main(int argc,char**argv){if(argc!=2)return 2;"
        "std::puts(\"ready\");std::fflush(stdout);"
        "for(unsigned i=0;i<28800;++i){"
        "if(GetFileAttributesA(argv[1])!=INVALID_FILE_ATTRIBUTES)return 0;"
        "Sleep(250);}return 3;}\n")
    with (output / "keepalive-build.log").open("wb") as build_log:
        subprocess.run(["i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall",
                        "-Wextra", "-Werror", "-static", str(source), "-o",
                        str(executable)], cwd=ROOT, env=env, check=True,
                       timeout=180, stdout=build_log, stderr=subprocess.STDOUT)
    marker = prefix / "drive_c" / f"a2fo-stage9-stop-{os.getpid()}"
    marker.unlink(missing_ok=True)
    log = log_path.open("wb")
    process = subprocess.Popen(["wine", str(executable), "C:\\" + marker.name],
                               cwd=ROOT, env=env, stdout=log, stderr=log)
    try:
        started = time.monotonic()
        while b"ready" not in log_path.read_bytes():
            if process.poll() is not None:
                raise RuntimeError("private headless Wine helper exited during startup")
            if time.monotonic() - started > 360:
                raise RuntimeError("private headless Wine helper startup exceeded 360 seconds")
            time.sleep(0.5)
    except BaseException:
        marker.touch()
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=30)
        log.close()
        marker.unlink(missing_ok=True)
        raise
    return process, log, marker, source, executable


def stop_keepalive(state):
    if not state:
        return
    process, log, marker, source, executable = state
    marker.touch()
    try:
        process.wait(timeout=30)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=30)
    finally:
        log.close()
        marker.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--wine-prefix", type=Path,
                        default=Path("/tmp/a2fo-shields-wine-prefix"))
    parser.add_argument("--phase", choices=("build", "native", "all"), default="all")
    parser.add_argument("--native-steps", nargs="+", choices=(
        "stage7-parity", "stage8-regressions", "builtin-native-tests", "stage10-consolidation"),
        help="select affected native suites; default runs all four")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    prefix = args.wine_prefix.resolve()
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        print(f"refusing nonempty output directory: {output}", file=sys.stderr)
        return 1
    output.mkdir(parents=True, exist_ok=True)
    receipt_path = output / "results.json"
    receipt = {"schema": 1, "started_at": utc_now(), "phase": args.phase,
               "wine_prefix": str(prefix), "status": "running", "steps": [],
               "selected_native_steps": args.native_steps or [
                   "stage7-parity", "stage8-regressions", "builtin-native-tests", "stage10-consolidation"]}
    write_json(receipt_path, receipt)
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(A2FO_BUILTIN_WINEPREFIX=str(prefix),
               A2FO_RUST_WINEPREFIX=str(prefix))
    keepalive = None
    try:
        if args.phase in ("build", "all"):
            for name, command in build_steps():
                run_step(name, command, output, receipt, BUILD_TIMEOUT, env, output)
        if args.phase in ("native", "all"):
            assert_archives_fresh()
            headless_env = wine_environment(prefix)
            keepalive = start_keepalive(prefix, output, headless_env)
            for name, command in native_steps(prefix, output):
                if args.native_steps and name not in args.native_steps:
                    continue
                run_step(name, command, output, receipt, NATIVE_TIMEOUT,
                         headless_env, output, native=True)
        receipt["status"] = "passed"
        result = 0
    except Exception as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        print(error, file=sys.stderr)
        result = 1
    finally:
        try:
            stop_keepalive(keepalive)
        except Exception as cleanup_error:
            receipt["status"] = "failed"
            receipt["cleanup_error"] = str(cleanup_error)
            result = 1
        receipt["finished_at"] = utc_now()
        write_json(receipt_path, receipt)
    return result


if __name__ == "__main__":
    sys.exit(main())
