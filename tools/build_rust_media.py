#!/usr/bin/env python3
"""Build the standalone Rust media DLL and record its exact source inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "rust/a2fo_media/Cargo.toml"
LOCK = MANIFEST.parent / "Cargo.lock"
VENDOR = ROOT / "third_party/rust-media"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_inputs() -> list[Path]:
    result = [MANIFEST, LOCK, ROOT / "rust/media_ffi.h", Path(__file__).resolve()]
    for base in (MANIFEST.parent / "src", VENDOR):
        for path in base.rglob("*"):
            if any(part in {"target", "targets", ".git"} for part in path.relative_to(base).parts):
                continue
            if path.is_file() and not path.is_symlink():
                result.append(path)
    return sorted(result)


def hashes(paths: list[Path]) -> dict[str, str]:
    return {str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path): sha256(path) for path in paths}


def dependency_inputs(target: str) -> list[Path]:
    metadata = json.loads(subprocess.check_output(
        ["cargo", "metadata", "--offline", "--locked", "--format-version", "1",
         "--manifest-path", str(MANIFEST), "--filter-platform", target], cwd=ROOT))
    result = []
    for package in metadata["packages"]:
        if not (package.get("source") or "").startswith("registry+"):
            continue
        base = Path(package["manifest_path"]).parent
        for path in base.rglob("*"):
            if any(part in {"target", ".git"} for part in path.relative_to(base).parts):
                continue
            if path.is_file() and not path.is_symlink():
                result.append(path)
    return sorted(set(result))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target", default="i686-pc-windows-gnu")
    parser.add_argument("--release", nargs="?", const="release", choices=("release", "debug"), default="release")
    args = parser.parse_args()

    target_dir = args.target_dir.resolve()
    output = args.output.resolve()
    if output.suffix.lower() != ".dll":
        parser.error("--output must name a DLL")
    if output.is_relative_to(MANIFEST.parent / "src") or output.is_relative_to(VENDOR):
        parser.error("output must not replace an input")
    files = source_inputs() + dependency_inputs(args.target)
    before = hashes(files)
    command = ["cargo", "build", "--locked", "--offline", "--manifest-path", str(MANIFEST),
               "--target", args.target, "--target-dir", str(target_dir)]
    if args.release == "release":
        command.append("--release")

    result = subprocess.run(command, cwd=ROOT, check=False)
    if result.returncode:
        print(f"cargo build failed ({result.returncode}); output preserved", file=sys.stderr)
        return result.returncode
    after = hashes(source_inputs() + dependency_inputs(args.target))
    if after != before:
        print("source changed during build; output preserved", file=sys.stderr)
        return 1

    built = target_dir / args.target / args.release / "a2fo_media.dll"
    if output == built:
        parser.error("--output must differ from Cargo's build artifact")
    if not built.is_file() or built.stat().st_size == 0:
        print(f"missing or empty built DLL: {built}; output preserved", file=sys.stderr)
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{output.name}.", suffix=".tmp", dir=output.parent)
    os.close(fd)
    temp = Path(temp_name)
    receipt_temp = None
    try:
        shutil.copyfile(built, temp)
        artifact_hash = sha256(temp)
        receipt = {
            "command": command,
            "target": args.target,
            "profile": args.release,
            "rustc": subprocess.check_output(["rustc", "--version", "--verbose"], text=True),
            "cargo": subprocess.check_output(["cargo", "--version"], text=True).strip(),
            "inputs": before,
            "output": str(output),
            "output_sha256": artifact_hash,
        }
        receipt_path = output.with_name(output.name + ".receipt.json")
        receipt_fd, receipt_name = tempfile.mkstemp(prefix=f".{receipt_path.name}.", suffix=".tmp", dir=output.parent)
        os.close(receipt_fd)
        receipt_temp = Path(receipt_name)
        receipt_temp.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temp, output)
        os.replace(receipt_temp, receipt_path)
    finally:
        temp.unlink(missing_ok=True)
        if receipt_temp is not None:
            receipt_temp.unlink(missing_ok=True)
    print(f"{output}: sha256 {artifact_hash}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except OSError as error:
        print(f"build_rust_media: {error}", file=sys.stderr)
        raise SystemExit(2)
