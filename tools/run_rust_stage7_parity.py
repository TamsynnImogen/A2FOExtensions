#!/usr/bin/env python3
"""Run the current stage 7 parity checks sequentially against pinned inputs."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage7"
WINDOWS_LIB = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
HOST_LIB = ROOT / "build/rust/release/liba2fo_core.a"

# Name, runner suffix, extra arguments, supported options. Keep this explicit so a
# newly added runner cannot silently change the final parity plan.
GROUPS = [
    ("queue_bridges_host", "queue_bridges", (), "prefix receipt"),
    ("queue_bridges_windows", "queue_bridges", ("--windows",), "prefix receipt"),
    ("intro", "intro", (), "prefix receipt"),
    ("inspector", "inspector", (), "prefix receipt"),
    ("icons", "icons", (), "prefix receipt"),
    ("refit_ui", "refit_ui", (), "prefix receipt"),
    ("ui", "ui", (), "archive prefix receipt"),
    ("resources", "resources", (), "archive prefix"),
    ("device", "device", (), "archive prefix"),
    ("backend", "backend", (), "archive prefix"),
    ("draw", "draw", (), "archive prefix"),
    ("runtime_a", "runtime_a", (), ""),
    ("runtime_a_combat", "runtime_a_combat", (), ""),
    ("runtime_a_team", "runtime_a_team", (), ""),
    ("runtime_a2_mailer", "runtime_a2_mailer", (), ""),
    ("runtime_a2_hybrid", "runtime_a2_hybrid", (), ""),
    ("runtime_a3_refit", "runtime_a3_refit", (), ""),
    ("runtime_a3_shell", "runtime_a3_shell", (), ""),
    ("runtime_a4", "runtime_a4", (), ""),
    ("runtime_a5_tooltip", "runtime_a5_tooltip", (), ""),
    ("runtime_a5_xp", "runtime_a5_xp", (), ""),
    ("runtime_a6_cheats", "runtime_a6_cheats", (), ""),
    ("runtime_b", "runtime_b", (), ""),
    ("runtime_b2", "runtime_b2", (), ""),
    ("runtime_b3", "runtime_b3", (), ""),
    ("runtime_b4", "runtime_b4", (), ""),
    ("runtime_b5", "runtime_b5", (), ""),
    ("runtime_b6", "runtime_b6", (), ""),
    ("shields_production_host", "shields_production", (), "prefix receipt"),
    ("shields_production_windows", "shields_production", ("--windows",), "prefix receipt"),
]


def now():
    return datetime.now(timezone.utc).isoformat()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def input_paths():
    scope = STAGE / "scope.json"
    units = json.loads(scope.read_text())["selected_units"]
    if len(units) != 37 or len(set(units)) != 37:
        raise ValueError(f"expected 37 unique stage 7 units, found {len(units)}")
    paths = {scope, *(ROOT / unit for unit in units)}
    paths.update(path for path in (ROOT / "rust").rglob("*")
                 if path.is_file() and (path.suffix == ".rs" or
                                        path.name.endswith("_ffi.h") or
                                        path.name in ("Cargo.toml", "Cargo.lock"))
                 and "target" not in path.parts)
    # Existing native fixtures include stage 6 and built-in tests, not only
    # rust_stage7_*. Hash all test inputs to cover their shared helpers too.
    paths.update(path for path in (ROOT / "tests").rglob("*")
                 if path.is_file() and "__pycache__" not in path.parts)
    paths.update((ROOT / "tools").glob("check_rust_stage7*_parity.py"))
    paths.add(Path(__file__).resolve())
    return paths


def snapshot(windows_lib):
    paths = input_paths()
    paths.update((windows_lib, HOST_LIB))
    missing = sorted(str(path) for path in paths if not path.is_file())
    if missing:
        raise FileNotFoundError("missing parity inputs: " + ", ".join(missing))
    return {str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path):
            sha256(path) for path in sorted(paths)}


def assert_archives_fresh(windows_lib):
    rust_inputs = [path for path in input_paths()
                   if path.is_relative_to(ROOT / "rust")]
    newest = max(rust_inputs, key=lambda path: path.stat().st_mtime_ns)
    for archive in (windows_lib, HOST_LIB):
        if newest.stat().st_mtime_ns > archive.stat().st_mtime_ns:
            raise RuntimeError(f"Rust input {newest} is newer than {archive}; "
                               "rebuild the archive before parity")


def changed(before, after):
    return sorted(key for key in before.keys() | after.keys()
                  if before.get(key) != after.get(key))


def write_receipt(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def command_for(group, windows_lib, wine_prefix, log_dir):
    name, suffix, extras, supported = group
    runner = ROOT / "tools" / f"check_rust_stage7_{suffix}_parity.py"
    command = [sys.executable, str(runner), *extras]
    if "archive" in supported:
        command.extend(("--rust-lib", str(windows_lib)))
    if "prefix" in supported:
        command.extend(("--wine-prefix", str(wine_prefix)))
    child_receipt = log_dir / f"{name}.receipt.json" if "receipt" in supported else None
    if child_receipt:
        command.extend(("--receipt", str(child_receipt)))
    return command, child_receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-lib", type=Path, default=WINDOWS_LIB)
    parser.add_argument("--wine-prefix", type=Path,
                        default=Path("/tmp/a2fo-shields-wine-prefix"))
    parser.add_argument("--only", action="append", choices=[group[0] for group in GROUPS],
                        help="run only this group; repeat to select several")
    parser.add_argument("--receipt", type=Path, default=STAGE / "final-parity.json")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    windows_lib = args.rust_lib.resolve()
    wine_prefix = args.wine_prefix.resolve()
    receipt_path = args.receipt.resolve()
    log_dir = receipt_path.with_suffix("").with_name(receipt_path.stem + "-logs")
    selection = set(args.only or ())
    plan = [group for group in GROUPS if not args.only or group[0] in selection]
    if len(plan) != len(selection) and args.only:
        parser.error("duplicate --only group")
    initial = snapshot(windows_lib)
    receipt = {"schema": 1, "started_at": now(), "status": "planned",
               "rust_lib": str(windows_lib), "host_rust_lib": str(HOST_LIB),
               "wine_prefix": str(wine_prefix), "selected_groups": [g[0] for g in plan],
               "initial_sha256": initial, "results": []}
    if args.dry_run:
        for group in plan:
            command, _ = command_for(group, windows_lib, wine_prefix, log_dir)
            print(group[0] + ": " + " ".join(command))
        print(f"DRY RUN: {len(plan)} groups; {len(initial)} pinned inputs")
        return 0

    log_dir.mkdir(parents=True, exist_ok=True)
    write_receipt(receipt_path, receipt)
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(A2FO_RUST_WIN_LIB=str(windows_lib),
               A2FO_RUST_WINEPREFIX=str(wine_prefix))
    try:
        assert_archives_fresh(windows_lib)
        for group in plan:
            name = group[0]
            drift = changed(initial, snapshot(windows_lib))
            if drift:
                raise RuntimeError(f"inputs changed before {name}: {drift}")
            command, child_receipt = command_for(group, windows_lib, wine_prefix,
                                                 log_dir)
            if child_receipt:
                child_receipt.unlink(missing_ok=True)
            stdout_path = log_dir / f"{name}.stdout.log"
            stderr_path = log_dir / f"{name}.stderr.log"
            start = time.monotonic()
            with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
                process = subprocess.run(command, cwd=ROOT, env=env,
                                         stdout=stdout, stderr=stderr, check=False)
            drift = changed(initial, snapshot(windows_lib))
            child_missing = bool(child_receipt and not child_receipt.is_file())
            result = {"group": name, "command": command,
                      "exit_code": process.returncode,
                      "duration_seconds": round(time.monotonic() - start, 3),
                      "stdout": str(stdout_path), "stderr": str(stderr_path),
                      "child_receipt": str(child_receipt) if child_receipt else None,
                      "child_receipt_missing": child_missing,
                      "changed_inputs": drift,
                      "status": "passed" if process.returncode == 0 and not drift
                                and not child_missing
                                else "failed"}
            receipt["results"].append(result)
            write_receipt(receipt_path, receipt)
            print(f"{name}: {result['status']} ({result['duration_seconds']}s)",
                  flush=True)
            if result["status"] != "passed":
                raise RuntimeError(f"{name} failed; see {stderr_path}")
        drift = changed(initial, snapshot(windows_lib))
        if drift:
            raise RuntimeError(f"inputs changed after final group: {drift}")
        receipt["status"] = "passed"
        return 0
    except Exception as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        print(error, file=sys.stderr)
        return 1
    finally:
        receipt["finished_at"] = now()
        write_receipt(receipt_path, receipt)


if __name__ == "__main__":
    sys.exit(main())
