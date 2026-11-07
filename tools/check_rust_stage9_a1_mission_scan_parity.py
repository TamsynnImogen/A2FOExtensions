#!/usr/bin/env python3
"""Compare frozen/current A1 mission scanning with a synthetic native reader."""

import argparse
from datetime import datetime, timezone
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage9"
WORK = STAGE / "a1-mission-scan-work"
UNIT = Path("modules/A1Compat/module.cpp")
FROZEN = ROOT / "build/rust-migration-stage8/before" / UNIT
BASELINE_MANIFEST = ROOT / "build/rust-migration-stage8/before-hashes.json"
TEST = ROOT / "tests/rust_stage9_a1_mission_scan_native.cpp"
STAGED_FIXTURE = STAGE / "a1-mission-scan-staged/mission_fixture.cpp"
ADAPTER = ROOT / "modules/A1Compat/a1_bzn_policy.cpp"
RUST_MANIFEST = ROOT / "rust/a2fo_core/Cargo.toml"
RUST_LOCK = ROOT / "rust/a2fo_core/Cargo.lock"
RUST_LIB = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
REQUIRED_EXPORTS = ("a2fo_rs_stage9_a1_mission_scan",)
SCENARIOS = (
    "header-native-fallback", "malformed-cursor", "empty", "short-record",
    "initial", "initial-short-circuit", "first-distance", "last-distance",
    "partial-after-boundary", "no-marker", "ambiguous", "read-only-slot",
    "native-load-fails", "campaign-record", "mutation-new-marker",
    "mutation-second-marker", "cap-last-complete", "cap-excluded",
)
CASE_LINE = re.compile(
    rb"^case:([^:]+):loaded=([01]):native=([01]):cursor=(\d+):"
    rb"probes=(\d+):last=(\d+):hash=([0-9a-f]{8}):logs=(\d+):counter=(-?\d+)$"
)


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def rust_inputs():
    paths = {RUST_MANIFEST, RUST_LOCK}
    paths.update((ROOT / "rust/a2fo_core/src").rglob("*.rs"))
    paths.update((ROOT / "rust").glob("*.h"))
    return paths


def pinned_paths(archive):
    paths = {
        ROOT / "Makefile", archive, FROZEN, BASELINE_MANIFEST,
        ROOT / UNIT, TEST, STAGED_FIXTURE, ADAPTER,
        Path(__file__).resolve(),
    }
    paths.update((ROOT / "modules/A1Compat").glob("*.cpp"))
    paths.update((ROOT / "modules/A1Compat").glob("*.hpp"))
    paths.update((ROOT / "sdk").rglob("*.h"))
    paths.update((ROOT / "sdk").rglob("*.hpp"))
    paths.update(rust_inputs())
    return paths


def snapshot(paths):
    missing = sorted(str(path) for path in paths if not path.is_file())
    if missing:
        raise FileNotFoundError("missing pinned inputs: " + ", ".join(missing))
    return {str(path): digest(path) for path in sorted(paths)}


def verify_inputs(archive):
    manifest = json.loads(BASELINE_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get(str(UNIT)) != digest(FROZEN):
        raise RuntimeError("frozen A1Compat source does not match before-hashes.json")
    newest = max(rust_inputs(), key=lambda path: path.stat().st_mtime_ns)
    if archive.stat().st_mtime_ns < newest.stat().st_mtime_ns:
        raise RuntimeError(f"Rust archive predates registered input {newest}")
    result = subprocess.run(["i686-w64-mingw32-nm", "-g", "--defined-only",
                             str(archive)], check=True, capture_output=True,
                            text=True, timeout=60)
    symbols = {line.split()[-1].lstrip("_") for line in result.stdout.splitlines()
               if line.split()}
    missing = sorted(set(REQUIRED_EXPORTS) - symbols)
    if missing:
        raise RuntimeError("Rust archive lacks required exports: " + ", ".join(missing))


def run_logged(command, label, work, timeout=300):
    stdout_path = work / f"{label}.stdout"
    stderr_path = work / f"{label}.stderr"
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr,
                                timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"{label} exited {result.returncode}; logs retained")
    return stdout_path, stderr_path


def source_for_fixture(source, work, label):
    text = source.read_text(encoding="utf-8")
    text = text.replace("__declspec(dllexport)", "")
    text = re.sub(
        r'#include "([^"\n]+)"',
        lambda match: '#include "' + str((source.parent / match.group(1)).resolve()) + '"',
        text,
    )
    generated = work / f"{label}-module.cpp"
    generated.write_text(text, encoding="utf-8")
    return generated


def compile_fixtures(archive, work, receipt):
    common = [
        "i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra",
        "-Wpedantic", "-Werror", "-Wno-error=cast-function-type",
        "-Wno-error=unused-function", "-DWIN32_LEAN_AND_MEAN", "-UNDEBUG",
        "-ffunction-sections", "-fdata-sections", "-static",
    ]
    adapter_object = work / "real-a1-bzn-policy.o"
    run_logged(common + ["-c", str(ADAPTER), "-o", str(adapter_object)],
               "adapter-build", work)
    nm = subprocess.run(["i686-w64-mingw32-nm", "--defined-only",
                         str(adapter_object)], check=True, capture_output=True,
                        text=True, timeout=60)
    adapter_symbols = {line.split()[-1] for line in nm.stdout.splitlines()
                       if line.split()}
    binaries = {}
    compile_logs = {}
    for label, source in (("frozen", FROZEN), ("current", ROOT / UNIT)):
        generated = source_for_fixture(source, work, label)
        object_file = work / f"{label}-mission.o"
        exe = work / f"{label}-mission.exe"
        run_logged(common + [f'-DA1_SOURCE="{generated}"', str(TEST), "-c",
                             "-o", str(object_file)], label + "-compile", work)
        undefined = subprocess.run(["i686-w64-mingw32-nm", "--undefined-only",
                                    str(object_file)], check=True,
                                   capture_output=True, text=True, timeout=60)
        native = []
        for line in undefined.stdout.splitlines():
            if not line.split():
                continue
            name = line.split()[-1]
            if (name.startswith(("_a2fo_a1_", "__ZN8a1compat", "__ZNK8a1compat"))
                    and name not in adapter_symbols):
                native.append(name)
        stubs = work / f"{label}-uninvoked.S"
        stubs.write_text(
            ".text\n" + "".join(
                ".globl " + name + "\n" + name +
                ":\n jmp _a1_mission_unexpected_native\n" for name in native),
            encoding="utf-8")
        trap = work / f"{label}-uninvoked.cpp"
        trap.write_text(
            "#include <cstdio>\n#include <cstdlib>\n"
            "extern \"C\" [[noreturn]] void a1_mission_unexpected_native()"
            "{std::fputs(\"unexpected unrelated A1 engine path\\n\",stderr);"
            "std::abort();}\n", encoding="utf-8")
        link = common + ["-Wl,--gc-sections", str(object_file),
                         str(adapter_object), str(stubs), str(trap), str(archive),
                         "-ladvapi32", "-lgdi32", "-lcomctl32", "-lversion",
                         "-lws2_32", "-lshell32", "-lwinmm", "-lole32",
                         "-lmsimg32", "-lgdiplus", "-ldxguid", "-o", str(exe)]
        out, err = run_logged(link, label + "-link", work)
        binaries[label] = exe
        compile_logs[label] = {"stdout": str(out), "stderr": str(err)}
    receipt["compile_logs"] = compile_logs
    receipt["binaries"] = {key: str(value) for key, value in binaries.items()}
    return binaries


def wine_environment(prefix):
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG="-all",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    return env


def run_native(exe, prefix, work, label):
    stdout_path = work / f"{label}.stdout"
    stderr_path = work / f"{label}.stderr"
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        try:
            result = subprocess.run(["wine", str(exe)], cwd=ROOT,
                                    env=wine_environment(prefix), stdout=stdout,
                                    stderr=stderr, timeout=180, check=False)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{label} timed out; stdout/stderr retained") from error
    if result.returncode:
        raise RuntimeError(f"{label} exited {result.returncode}; traces retained")
    return stdout_path.read_bytes(), stderr_path.read_bytes()


def validate_trace(data):
    lines = data.splitlines()
    marker = b"A1 mission actual-source scope:18 scenarios +null guards PASS"
    if len(lines) != 36 or lines.count(marker) != 1:
        raise RuntimeError("A1 trace must contain 36 rows and the 18-scenario marker")
    case_rows = [line for line in lines if line.startswith(b"case:")]
    log_rows = [line for line in lines if line.startswith(b"log:")]
    if len(case_rows) != 18 or len(log_rows) != 17:
        raise RuntimeError("A1 trace must contain 18 case rows and 17 native log rows")
    found = []
    for row in case_rows:
        match = CASE_LINE.fullmatch(row)
        if not match:
            raise RuntimeError(f"invalid A1 probe/cursor result row: {row!r}")
        name = match.group(1).decode("ascii")
        if int(match.group(3)) != 1:
            raise RuntimeError(f"A1 native reader was not called once for {name}")
        found.append(name)
    if tuple(found) != SCENARIOS:
        raise RuntimeError("A1 scenario order does not match the fixture")
    return {"scenarios": len(case_rows), "output_lines": len(lines),
            "probe_cursor_rows": len(case_rows), "native_log_rows": len(log_rows)}


def write_first_diff(path, before, after):
    sections = []
    for name, left, right in (("stdout", before[0], after[0]),
                              ("stderr", before[1], after[1])):
        left_lines = left.decode("utf-8", errors="replace").splitlines(keepends=True)
        right_lines = right.decode("utf-8", errors="replace").splitlines(keepends=True)
        sections.append(f"=== {name} ===\n")
        sections.extend(difflib.unified_diff(left_lines, right_lines,
                       fromfile=f"frozen/{name}", tofile=f"current/{name}"))
    path.write_text("".join(sections), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-lib", type=Path,
                        default=os.environ.get("A2FO_RUST_WIN_LIB", RUST_LIB))
    parser.add_argument("--wine-prefix", type=Path,
                        default=os.environ.get("A2FO_RUST_WINEPREFIX",
                            os.environ.get("WINEPREFIX", "/tmp/a2fo-shields-wine-prefix")))
    parser.add_argument("--work-dir", type=Path, default=WORK)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    work = args.work_dir.resolve()
    if work.exists() and any(work.iterdir()):
        print(f"refusing nonempty work directory: {work}", file=sys.stderr)
        return 1
    work.mkdir(parents=True, exist_ok=True)
    receipt = {"unit": str(UNIT), "status": "running", "rust_archive": str(archive),
               "work_dir": str(work), "build_only": args.build_only,
               "started_at": datetime.now(timezone.utc).isoformat()}
    before = after = {}
    try:
        paths = pinned_paths(archive)
        before = snapshot(paths)
        verify_inputs(archive)
        binaries = compile_fixtures(archive, work, receipt)
        if args.build_only:
            receipt["status"] = "built"
        else:
            traces = {}
            for label, exe in binaries.items():
                traces[label] = run_native(exe, args.wine_prefix.resolve(), work, label)
            if traces["frozen"] != traces["current"]:
                write_first_diff(work / "firstdiff.txt", traces["frozen"],
                                 traces["current"])
                raise RuntimeError("A1 mission scan frozen/current trace mismatch")
            for label, trace in traces.items():
                receipt[f"{label}_results"] = validate_trace(trace[0])
            receipt.update(status="passed", scenarios=18,
                           frozen_sha256=digest(FROZEN),
                           current_sha256=digest(ROOT / UNIT))
        after = snapshot(paths)
        changed = sorted(path for path in before.keys() | after.keys()
                         if before.get(path) != after.get(path))
        receipt["changed_pins"] = changed
        if changed:
            receipt["status"] = "failed"
            raise RuntimeError("pinned inputs changed during run: " + ", ".join(changed))
        return_code = 0
    except Exception as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        print(error, file=sys.stderr)
        return_code = 1
    finally:
        receipt["pins_before"] = before
        try:
            after = snapshot(pinned_paths(archive))
            receipt["pins_after"] = after
        except Exception as error:
            receipt["snapshot_error"] = str(error)
            receipt["status"] = "failed"
            return_code = 1
        receipt["finished_at"] = datetime.now(timezone.utc).isoformat()
        (work / "receipt.json").write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"A1 mission scan parity: {receipt['status']}")
    return return_code


if __name__ == "__main__":
    sys.exit(main())
