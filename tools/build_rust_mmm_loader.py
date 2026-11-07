#!/usr/bin/env python3
"""Build the standalone Rust MMM loader DLL with a pinned receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "rust/a2fo_mmm_loader"
DEFAULT_SOURCE = SOURCE_DIR / "lib.rs"
DEFINITION = ROOT / "missions/mmm_loader/mmm_loader.def"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inside_root(path: Path) -> bool:
    try:
        path.relative_to(ROOT)
        return True
    except ValueError:
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    source = (ROOT / args.source).resolve() if not args.source.is_absolute() else args.source.resolve()
    output = args.output.resolve()
    if output.suffix != ".dll":
        raise ValueError("output path must end in .dll")
    if not inside_root(output):
        raise ValueError("output must resolve inside the repository")
    if not inside_root(source) or not source.is_file():
        raise ValueError(f"source must exist inside the repository: {source}")
    if not source.is_relative_to(SOURCE_DIR):
        raise ValueError(f"source must be inside {SOURCE_DIR.relative_to(ROOT)}")
    if not DEFINITION.is_file():
        raise ValueError(f"link definition does not exist: {DEFINITION}")
    if output == source or output == DEFINITION:
        raise ValueError("output must not be an input file")

    source_files = sorted(SOURCE_DIR.rglob("*.rs"))
    if not source_files:
        raise ValueError(f"no Rust inputs found in {SOURCE_DIR}")
    if source not in source_files:
        raise ValueError("selected source must be one of the pinned Rust inputs")
    inputs = [*source_files, DEFINITION, Path(__file__).resolve()]
    inputs_before = {str(path.relative_to(ROOT)): sha256(path) for path in inputs}
    output.parent.mkdir(parents=True, exist_ok=True)

    rustc = shlex.split(os.environ.get("RUSTC", "rustc"))
    if not rustc:
        raise ValueError("RUSTC is empty")
    version = subprocess.run([*rustc, "--version"], capture_output=True, text=True, check=False)
    rust_version = (version.stdout or version.stderr).strip()
    if version.returncode:
        raise RuntimeError(f"rustc --version failed with exit code {version.returncode}")

    fd, temp_name = tempfile.mkstemp(prefix=f".{output.name}.", suffix=".tmp.dll", dir=output.parent)
    os.close(fd)
    temp = Path(temp_name)
    command = [
        *rustc, "--edition=2024", "--crate-type=cdylib", "--target", "i686-pc-windows-gnu",
        "-Cpanic=abort", "-Copt-level=2", "-Ccodegen-units=1", "-Cdebuginfo=0",
        f"-Clink-arg={DEFINITION}", "-Clink-arg=-static", str(source), "-o", str(temp),
    ]
    log_path = output.with_name(output.name + ".log")
    receipt_path = output.with_name(output.name + ".receipt.json")
    receipt = {
        "status": "running", "exit_code": None, "inputs_before": inputs_before,
        "inputs_after": None, "command": command, "rust_version": rust_version,
        "output": str(output), "temporary_output": str(temp), "log": str(log_path),
        "artifact_sha256": None,
    }

    def write_receipt() -> None:
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    try:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        log_path.write_bytes(result.stdout)
        receipt["exit_code"] = result.returncode
        if result.returncode:
            receipt["status"] = "failed"
            raise RuntimeError(f"rustc failed with exit code {result.returncode}; see {log_path}")
        inputs_after = {name: sha256(ROOT / name) for name in inputs_before}
        receipt["inputs_after"] = inputs_after
        if inputs_after != inputs_before:
            receipt["status"] = "failed"
            raise RuntimeError("an input changed during compilation; output was not replaced")
        if not temp.is_file() or temp.stat().st_size == 0:
            receipt["status"] = "failed"
            raise RuntimeError("rustc succeeded but did not produce a non-empty DLL")
        os.replace(temp, output)
        receipt["status"] = "success"
        receipt["artifact_sha256"] = sha256(output)
        return 0
    except (OSError, RuntimeError) as error:
        receipt.setdefault("error", str(error))
        if not log_path.exists():
            log_path.write_text(f"{error}\n", encoding="utf-8")
        if receipt["status"] == "running":
            receipt["status"] = "failed"
        print(f"build_rust_mmm_loader: {error}", file=sys.stderr)
        return 1
    finally:
        write_receipt()
        temp.unlink(missing_ok=True)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"build_rust_mmm_loader: {error}", file=sys.stderr)
        raise SystemExit(2)
