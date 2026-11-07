#!/usr/bin/env python3
"""Compare frozen/current EnergySystems range admission on the canonical Rust archive."""

import argparse
from collections import Counter
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
WORK = ROOT / "build/rust-migration-stage9/energy-numeric-work"
ARCHIVE_SHA256 = "a40ebbd9ccc49e308ad7ad116a33759e9d432c5f7ea1d19797188c5942b43a37"
UNIT = Path("modules/A2FOEnergySystems/module.cpp")
FROZEN = ROOT / "build/rust-migration-stage9/batch6-before" / UNIT
MANIFEST = ROOT / "build/rust-migration-stage9/energy-range-staged/source-hashes.json"
BASELINE_MANIFEST = ROOT / "build/rust-migration-stage9/batch6-before/hashes.json"
BASELINE_SHA256 = "cb7da16e7428d1802a190a98cc8fc582e108fe3919d542c6b15846dcd80c3608"
STAGED_SHA256 = "b2bb7b3dc0e9414c1640149ccc7305cdaa331ee8de5f6e9321d7af239e0e7181"
STAGED_SUPPLEMENT = ROOT / "build/rust-migration-stage9/energy-range-staged/range_supplement.cpp"
STAGED_SUPPLEMENT_SHA256 = "15cdc2eb4fd0d03306e55c1bdb0ec36802bea5b0e7014dd8e8050432c2ae86e6"
TEST = ROOT / "tests/rust_stage9_energy_numeric_native.cpp"
SUPPORT_TEST = ROOT / "tests/rust_stage8_a_energy_native.cpp"
ENERGY_ADAPTER = ROOT / "modules/A2FOEnergySystems/energy_systems.cpp"
RESUPPLY_ADAPTER = ROOT / "modules/A2FOEnergySystems/resupply_policy.cpp"
REQUIRED_EXPORTS = ("a2fo_rs_stage9_energy_range_admit",)
PINNED_INPUTS = (
    "Makefile",
    "rust/a2fo_core/src/lib.rs",
    "rust/a2fo_core/src/energy.rs",
    "rust/a2fo_core/src/stage8_a_energy.rs",
    "rust/a2fo_core/src/stage9_energy_numeric.rs",
    "rust/energy_ffi.h",
    "rust/resupply_ffi.h",
    "rust/stage8_a_energy_ffi.h",
    "rust/stage9_energy_numeric_ffi.h",
    "modules/A2FOEnergySystems/energy_systems.hpp",
    "modules/A2FOEnergySystems/resupply_policy.hpp",
    "sdk/include/a2fo_module_api.h",
    "sdk/include/a2fo_module_provider.hpp",
)
TRACE_LINE = re.compile(
    r"^(pc24|pc53|pc64)/(near|down|up|chop) (\d+) "
    r"(?:[0-9a-f]{8} ){7}([01]) cw=([0-9a-f]{4}) "
    r"top=([0-7]) flags=([0-9a-f]{2})$"
)


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def pin_paths(archive):
    return {ROOT / path for path in PINNED_INPUTS} | {
        archive, FROZEN, MANIFEST, BASELINE_MANIFEST, STAGED_SUPPLEMENT,
        ROOT / UNIT, TEST,
        SUPPORT_TEST, ENERGY_ADAPTER, RESUPPLY_ADAPTER,
        Path(__file__).resolve(),
    }


def snapshot(paths):
    missing = sorted(str(path) for path in paths if not path.is_file())
    if missing:
        raise FileNotFoundError("missing pinned inputs: " + ", ".join(missing))
    return {str(path): digest(path) for path in sorted(paths)}


def verify_inputs(archive):
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    baseline_manifest = json.loads(BASELINE_MANIFEST.read_text(encoding="utf-8"))
    if (baseline_manifest.get(str(UNIT)) != BASELINE_SHA256 or
            manifest.get("original") != BASELINE_SHA256 or
            manifest.get("staged") != STAGED_SHA256 or
            digest(FROZEN) != BASELINE_SHA256 or
            digest(STAGED_SUPPLEMENT) != STAGED_SUPPLEMENT_SHA256):
        raise RuntimeError("frozen EnergySystems source or staged v4 manifest pin changed")
    if digest(archive) != ARCHIVE_SHA256:
        raise RuntimeError(f"Rust archive is not the held canonical archive: {archive}")
    result = subprocess.run(["nm", "-g", "--defined-only", str(archive)],
                            check=True, capture_output=True, text=True, timeout=60)
    symbols = {line.split()[-1].lstrip("_") for line in result.stdout.splitlines()
               if line.split()}
    missing = sorted(set(REQUIRED_EXPORTS) - symbols)
    if missing:
        raise RuntimeError("Rust archive lacks required exports: " + ", ".join(missing))


def compile_fixture(source, archive, exe, stdout_path, stderr_path):
    command = [
        "i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra",
        "-Wpedantic", "-Werror", "-UNDEBUG", "-ffunction-sections",
        "-fdata-sections", "-Wl,--gc-sections", "-static",
        f'-DMODULE_SOURCE="{source}"', "-I", str(ROOT / "modules/A2FOEnergySystems"),
        str(TEST), str(ENERGY_ADAPTER), str(RESUPPLY_ADAPTER), str(archive),
        "-lws2_32", "-lbcrypt", "-lole32", "-ladvapi32", "-o", str(exe),
    ]
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr,
                                timeout=240, check=False)
    if result.returncode:
        raise RuntimeError(f"fixture compile failed ({result.returncode}); see {stderr_path}")


def wine_environment(prefix):
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG="-all",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    return env


def run_fixture(exe, prefix, stdout_path, stderr_path):
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        try:
            result = subprocess.run(["wine", str(exe)], cwd=ROOT,
                                    env=wine_environment(prefix), stdout=stdout,
                                    stderr=stderr, timeout=300, check=False)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{exe.name} timed out; stdout/stderr retained") from error
    if result.returncode:
        raise RuntimeError(f"{exe.name} exited {result.returncode}; stdout/stderr retained")
    return stdout_path.read_bytes().replace(b"\r\n", b"\n"), \
        stderr_path.read_bytes().replace(b"\r\n", b"\n")


def validate_trace(stdout):
    counts = Counter()
    admissions = Counter()
    control_words = set()
    tops = set()
    status_flags = Counter()
    sequences = {}
    lines = stdout.decode("ascii").splitlines()
    for line in lines:
        match = TRACE_LINE.fullmatch(line)
        if not match:
            raise RuntimeError(f"unexpected energy range trace row: {line!r}")
        precision, rounding, raw_number, admitted, control_word, top, flags = match.groups()
        key = f"{precision}/{rounding}"
        number = int(raw_number)
        if number != sequences.get(key, 0):
            raise RuntimeError(f"non-sequential case number in {key}: {number}")
        sequences[key] = number + 1
        counts[key] += 1
        admissions[admitted] += 1
        control_words.add(control_word)
        tops.add(top)
        status_flags[flags] += 1
    expected_keys = {f"{precision}/{rounding}"
                     for precision in ("pc24", "pc53", "pc64")
                     for rounding in ("near", "down", "up", "chop")}
    if len(lines) != 6600 or set(counts) != expected_keys or any(
            count != 550 for count in counts.values()):
        raise RuntimeError("energy range trace must contain 6600 rows (550 per CW mode)")
    return {
        "cases": len(lines),
        "cases_by_control_mode": dict(sorted(counts.items())),
        "admission_results": dict(sorted(admissions.items())),
        "control_words": sorted(control_words),
        "x87_top_values": sorted(tops),
        "status_flags": dict(sorted(status_flags.items())),
    }


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
                        default=os.environ.get("A2FO_RUST_WIN_LIB",
                            ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"))
    parser.add_argument("--wine-prefix", type=Path,
                        default=os.environ.get("A2FO_RUST_WINEPREFIX",
                            os.environ.get("WINEPREFIX", "/tmp/a2fo-shields-wine-prefix")))
    parser.add_argument("--work-dir", type=Path, default=WORK)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    receipt = {"unit": str(UNIT), "status": "running", "started_at":
               datetime.now(timezone.utc).isoformat(), "rust_archive": str(archive),
               "build_only": args.build_only}
    before = after = {}
    try:
        paths = pin_paths(archive)
        before = snapshot(paths)
        verify_inputs(archive)
        binaries = {}
        receipt["compile_logs"] = {}
        for version, source in (("frozen", FROZEN), ("current", ROOT / UNIT)):
            exe = work / f"energy-numeric-{version}.exe"
            stdout_path = work / f"{version}-build.stdout"
            stderr_path = work / f"{version}-build.stderr"
            compile_fixture(source, archive, exe,
                            stdout_path, stderr_path)
            binaries[version] = exe
            receipt["compile_logs"][version] = {
                "stdout": str(stdout_path), "stderr": str(stderr_path)}
        receipt["binaries"] = {name: str(path) for name, path in binaries.items()}
        if args.build_only:
            receipt["status"] = "built"
        else:
            traces = {}
            for version, exe in binaries.items():
                traces[version] = run_fixture(
                    exe, args.wine_prefix.resolve(),
                    work / f"{version}.stdout", work / f"{version}.stderr")
            if traces["frozen"] != traces["current"]:
                write_first_diff(work / "firstdiff.txt", traces["frozen"],
                                 traces["current"])
                raise RuntimeError("EnergySystems range frozen/current trace mismatch")
            receipt["frozen_results"] = validate_trace(traces["frozen"][0])
            receipt["current_results"] = validate_trace(traces["current"][0])
            receipt.update(status="passed", cases=6600,
                           frozen_sha256=digest(FROZEN),
                           current_sha256=digest(ROOT / UNIT))
        after = snapshot(paths)
        changed = sorted(path for path in before.keys() | after.keys()
                         if before.get(path) != after.get(path))
        receipt["changed_pins"] = changed
        if changed:
            receipt["status"] = "failed"
            raise RuntimeError("pinned inputs changed during run: " + ", ".join(changed))
    except Exception as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        print(error, file=sys.stderr)
        return_code = 1
    else:
        return_code = 0
    finally:
        receipt["pins_before"] = before
        try:
            after = snapshot(pin_paths(archive))
        except Exception as error:
            receipt["snapshot_error"] = str(error)
            receipt["status"] = "failed"
            return_code = 1
        receipt["pins_after"] = after
        receipt["finished_at"] = datetime.now(timezone.utc).isoformat()
        (work / "receipt.json").write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"EnergySystems range parity: {receipt['status']}")
    return return_code


if __name__ == "__main__":
    sys.exit(main())
