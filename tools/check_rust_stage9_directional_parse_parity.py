#!/usr/bin/env python3
"""Compare frozen/current DirectionalShields parsing on the canonical Rust archive."""

import argparse
from datetime import datetime, timezone
import difflib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/rust-migration-stage9/directional-parse-work"
ARCHIVE_SHA256 = "a40ebbd9ccc49e308ad7ad116a33759e9d432c5f7ea1d19797188c5942b43a37"
UNIT = Path("modules/A2FODirectionalShields/module.cpp")
FROZEN = ROOT / "build/rust-migration-stage9/batch6-before" / UNIT
MANIFEST = ROOT / "build/rust-migration-stage9/batch6-before/hashes.json"
BASELINE_SHA256 = "5cc564f033d7803db7648d567defb168ddecd5820370c82eb6b6b011da8365e6"
STAGED_SUPPLEMENT = ROOT / "build/rust-migration-stage9/directional-parse-staged/parse_supplement.cpp"
STAGED_SUPPLEMENT_SHA256 = "f722eb94b8bbe421000c6e4baae22044b3f0c38df5d127e8b18da08c39b78dc4"
TEST = ROOT / "tests/rust_stage9_directional_parse_native.cpp"
SUPPORT_TEST = ROOT / "tests/rust_stage8_a_directional_native.cpp"
ADAPTER = ROOT / "modules/A2FODirectionalShields/directional_shields.cpp"
REQUIRED_EXPORTS = (
    "a2fo_rs_stage9_directional_field_value",
    "a2fo_rs_stage9_directional_float_suffix",
)
PRECISION_CONTROL_WORDS = {
    "131072": "_PC_24",
    "65536": "_PC_53",
    "0": "_PC_64",
}
PINNED_INPUTS = (
    "Makefile",
    "rust/a2fo_core/src/lib.rs",
    "rust/a2fo_core/src/shields.rs",
    "rust/a2fo_core/src/stage8_a_directional.rs",
    "rust/a2fo_core/src/stage9_directional_parse.rs",
    "rust/shields_ffi.h",
    "rust/stage8_a_directional_ffi.h",
    "rust/stage9_directional_parse_ffi.h",
    "modules/A2FODirectionalShields/api.hpp",
    "modules/A2FODirectionalShields/directional_shields.hpp",
    "sdk/include/a2fo_module_api.h",
    "sdk/include/a2fo_module_provider.hpp",
    "sdk/include/a2fo_shield_values.hpp",
)


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def pin_paths(archive):
    return {ROOT / path for path in PINNED_INPUTS} | {
        archive, FROZEN, MANIFEST, STAGED_SUPPLEMENT, ROOT / UNIT, TEST,
        SUPPORT_TEST, ADAPTER,
        Path(__file__).resolve(),
    }


def snapshot(paths):
    missing = sorted(str(path) for path in paths if not path.is_file())
    if missing:
        raise FileNotFoundError("missing pinned inputs: " + ", ".join(missing))
    return {str(path): digest(path) for path in sorted(paths)}


def verify_inputs(archive):
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if (manifest.get(str(UNIT)) != BASELINE_SHA256 or
            digest(FROZEN) != BASELINE_SHA256 or
            digest(STAGED_SUPPLEMENT) != STAGED_SUPPLEMENT_SHA256):
        raise RuntimeError("frozen DirectionalShields source or batch6 manifest pin changed")
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
        f'-DMODULE_SOURCE="{source}"', "-I", str(ROOT / "modules/A2FODirectionalShields"),
        str(TEST), str(ADAPTER), str(archive), "-lws2_32", "-lbcrypt",
        "-lole32", "-ladvapi32", "-o", str(exe),
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
                                    stderr=stderr, timeout=180, check=False)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{exe.name} timed out; stdout/stderr retained") from error
    if result.returncode:
        raise RuntimeError(f"{exe.name} exited {result.returncode}; stdout/stderr retained")
    return stdout_path.read_bytes().replace(b"\r\n", b"\n"), \
        stderr_path.read_bytes().replace(b"\r\n", b"\n")


def validate_trace(stdout):
    lines = stdout.decode("ascii").splitlines()
    if len(lines) != 6:
        raise RuntimeError(f"directional parser trace has {len(lines)} lines; expected 6")
    required = {
        "field-order:first-valid-duplicate",
        "field-exception:typed-bad-alloc:noexcept-contained",
        "float-suffix:ascii-embedded-nul-overflow-finite",
    }
    if not required.issubset(lines):
        raise RuntimeError("directional parser trace is missing a required result marker")
    precision_lines = [line for line in lines if line.startswith("float-precision:")]
    if len(precision_lines) != len(PRECISION_CONTROL_WORDS):
        raise RuntimeError("directional parser trace lacks three precision results")
    observed = {}
    for line in precision_lines:
        fields = line.split(":")
        if (len(fields) != 6 or fields[1] not in PRECISION_CONTROL_WORDS or
                fields[2] != "negative-zero" or fields[4] != "subnormal"):
            raise RuntimeError(f"unknown precision control-word result: {line!r}")
        if fields[1] in observed:
            raise RuntimeError(f"duplicate precision control-word result: {line!r}")
        observed[fields[1]] = {
            "control_word": PRECISION_CONTROL_WORDS[fields[1]],
            "negative_zero_bits": fields[3],
            "subnormal_bits": fields[5],
        }
    if set(observed) != set(PRECISION_CONTROL_WORDS):
        raise RuntimeError("directional parser trace lacks exact _PC_24/_PC_53/_PC_64 results")
    return {"cases": 6, "precision_results": observed}


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
            exe = work / f"directional-parse-{version}.exe"
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
                raise RuntimeError("DirectionalShields parser frozen/current trace mismatch")
            for version, trace in traces.items():
                receipt[f"{version}_results"] = validate_trace(trace[0])
            receipt.update(status="passed", cases=6,
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
    print(f"DirectionalShields parser parity: {receipt['status']}")
    return return_code


if __name__ == "__main__":
    sys.exit(main())
