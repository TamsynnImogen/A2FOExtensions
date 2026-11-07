#!/usr/bin/env python3
"""Compile one explicitly registered Rust bridge source into an object file."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "rust/a2fo_bridges/manifest.json"


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


def registered_source(argument: Path) -> tuple[Path, Path, dict[str, str]]:
    source = (ROOT / argument).resolve() if not argument.is_absolute() else argument.resolve()
    if not inside_root(source):
        raise ValueError("source must resolve inside the repository")
    if not source.is_file():
        raise ValueError(f"source does not exist: {source}")
    manifest_data = MANIFEST.read_bytes()
    manifest = json.loads(manifest_data)
    if not isinstance(manifest, dict) or manifest.get("schema") != 1 or not isinstance(manifest.get("bridges"), list):
        raise ValueError("bridge manifest must have schema 1 and a bridges array")
    registered = []
    for entry in manifest["bridges"]:
        if not isinstance(entry, dict) or not isinstance(entry.get("legacy"), str) or not isinstance(entry.get("rust"), str):
            raise ValueError("malformed bridge manifest entry")
        rust_path = Path(entry["rust"])
        if rust_path.is_absolute():
            raise ValueError("manifest Rust paths must be repository-relative")
        registered.append((ROOT / rust_path).resolve())
    if source not in registered:
        raise ValueError(f"source is not registered in {MANIFEST.relative_to(ROOT)}: {source.relative_to(ROOT)}")
    return source, MANIFEST, {"sha256": hashlib.sha256(manifest_data).hexdigest()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    output = args.output.resolve()
    if output.suffix != ".o":
        raise ValueError("output path must end in .o")
    if not inside_root(output):
        raise ValueError("output must resolve inside the repository")
    source, manifest_path, manifest_hash = registered_source(args.source)
    if output == source:
        raise ValueError("output must not be the input source")
    output.parent.mkdir(parents=True, exist_ok=True)

    tool_path = Path(__file__).resolve()
    inputs_before = {
        str(source.relative_to(ROOT)): sha256(source),
        str(manifest_path.relative_to(ROOT)): manifest_hash["sha256"],
        str(tool_path.relative_to(ROOT)): sha256(tool_path),
    }
    rustc = os.environ.get("RUSTC", "rustc")
    rustc_args = shlex.split(rustc)
    if not rustc_args:
        raise ValueError("RUSTC is empty")
    version_result = subprocess.run([*rustc_args, "--version"], capture_output=True, text=True, check=False)
    rust_version = (version_result.stdout or version_result.stderr).strip()
    if version_result.returncode != 0:
        raise RuntimeError(f"rustc --version failed with exit code {version_result.returncode}")

    key = str(source.relative_to(ROOT).with_suffix("")).replace("/", "_")
    key = re.sub(r"[^A-Za-z0-9_]", "_", key)
    command = [
        *rustc_args, "--edition=2024", "--crate-type=lib", "--emit=obj",
        "--target", "i686-pc-windows-gnu", "-Cpanic=abort", "-Copt-level=2",
        "-Ccodegen-units=1", "-Cdebuginfo=0", "--crate-name", f"a2fo_bridge_{key}",
        str(source), "-o",
    ]
    fd, temp_name = tempfile.mkstemp(prefix=f".{output.name}.", suffix=".tmp.o", dir=output.parent)
    os.close(fd)
    temp = Path(temp_name)
    command.append(str(temp))
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
        if result.returncode != 0:
            receipt["status"] = "failed"
            raise RuntimeError(f"rustc failed with exit code {result.returncode}; see {log_path}")
        inputs_after = {name: sha256(ROOT / name) for name in inputs_before}
        receipt["inputs_after"] = inputs_after
        if inputs_after != inputs_before:
            receipt["status"] = "failed"
            raise RuntimeError("an input changed during compilation; output was not replaced")
        if not temp.is_file():
            receipt["status"] = "failed"
            raise RuntimeError("rustc succeeded but did not produce the requested object")
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
        print(f"build_rust_bridge: {error}", file=sys.stderr)
        return 1
    finally:
        write_receipt()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"build_rust_bridge: {error}", file=sys.stderr)
        raise SystemExit(2)
