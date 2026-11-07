#!/usr/bin/env python3
"""Build the Rust startup proxy DLL with the pinned MSVC target toolchain."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT / "rust/a2fo_startup_proxy"
EXPORTS = SOURCE_DIR / "exports.def"
DEFAULT_RUSTC = Path(
    "/home/tamsynn/.rustup/toolchains/1.96.0-x86_64-unknown-linux-gnu/bin/rustc"
)
TARGET = "i686-pc-windows-msvc"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_output_path(value: str, parser: argparse.ArgumentParser) -> Path:
    raw = Path(value)
    if ".." in raw.parts:
        parser.error("--output must not contain parent traversal")
    candidate = raw if raw.is_absolute() else ROOT / raw
    build_root = ROOT / "build"
    try:
        relative = candidate.relative_to(build_root)
    except ValueError:
        parser.error("--output must be inside the repository build/ directory")
    if not relative.parts or candidate.suffix.lower() != ".dll":
        parser.error("--output must name a .dll inside build/")
    current = build_root
    if current.is_symlink():
        parser.error("build/ must not be a symlink")
    for component in relative.parts[:-1]:
        current = current / component
        if current.is_symlink():
            parser.error("--output parent path must not traverse a symlink")
    if candidate.is_symlink():
        parser.error("--output must not be a symlink")
    candidate.parent.mkdir(parents=True, exist_ok=True)
    resolved = candidate.parent.resolve() / candidate.name
    try:
        resolved.relative_to(build_root.resolve())
    except ValueError:
        parser.error("resolved --output must remain inside build/")
    if resolved.is_symlink():
        parser.error("--output must not be a symlink")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="DLL path inside repository build/")
    args = parser.parse_args()
    output = safe_output_path(args.output, parser)
    sources = sorted(SOURCE_DIR.rglob("*.rs"))
    if not sources or not EXPORTS.is_file():
        parser.error("startup proxy Rust sources and exports.def must exist")
    inputs = [*sources, EXPORTS, Path(__file__).resolve()]
    for path in inputs:
        if path.is_symlink() or not path.is_file():
            parser.error(f"input must be a regular non-symlink file: {path}")
        if path.resolve() in (output, output.with_suffix(output.suffix + ".map"),
                              Path(str(output) + ".log"), Path(str(output) + ".receipt.json")):
            parser.error("output artifacts must not overlap source inputs")

    rustc_value = os.environ.get("RUSTC")
    if rustc_value:
        compiler_parts = shlex.split(rustc_value)
        if len(compiler_parts) != 1:
            parser.error("RUSTC must name exactly one direct compiler executable")
        rustc = Path(compiler_parts[0]).expanduser().resolve()
    else:
        rustc = DEFAULT_RUSTC
    if not rustc.is_file() or not os.access(rustc, os.X_OK):
        parser.error(f"rustc executable is unavailable: {rustc}")
    toolchain = rustc.parent.parent
    linker = toolchain / "lib/rustlib/x86_64-unknown-linux-gnu/bin/rust-lld"
    if not linker.is_file() or not os.access(linker, os.X_OK):
        parser.error(f"rust-lld executable is unavailable: {linker}")

    map_path = Path(str(output) + ".map")
    log_path = Path(str(output) + ".log")
    receipt_path = Path(str(output) + ".receipt.json")
    before = {str(path): sha256(path) for path in inputs}
    with tempfile.NamedTemporaryFile(prefix=f".{output.stem}.", suffix=".tmp.dll",
                                     dir=output.parent, delete=False) as temp:
        temporary_output = Path(temp.name)
    command = [
        str(rustc), "--target", TARGET, "--edition=2024", "--crate-type=cdylib",
        "-Cpanic=abort", "-Copt-level=2", "-Ccodegen-units=1", "-Cdebuginfo=0",
        "-Clto=fat", "-Cembed-bitcode=yes",
        f"-Clinker={linker}", "-Clinker-flavor=lld-link", "-Clink-arg=/nodefaultlib",
        "-Clink-arg=/entry:DllMain", f"-Clink-arg=/def:{EXPORTS.resolve()}",
        f"-Clink-arg=/map:{map_path}", str(SOURCE_DIR / "lib.rs"), "-o", str(temporary_output),
    ]
    version_command = [str(rustc), "--version"]
    stdout_parts = []
    stderr_parts = []
    try:
        version_result = subprocess.run(version_command, cwd=ROOT, text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        stdout_parts.append("$ " + shlex.join(version_command) + "\n" + version_result.stdout)
        stderr_parts.append(version_result.stderr)
        result = subprocess.run(command, cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        stdout_parts.append("$ " + shlex.join(command) + "\n" + result.stdout)
        stderr_parts.append(result.stderr)
        after = {str(path): sha256(path) for path in inputs}
        changed = [name for name, digest in before.items() if after[name] != digest]
        exit_code = result.returncode or (1 if version_result.returncode or changed else 0)
        if exit_code == 0 and temporary_output.is_file() and map_path.is_file():
            os.replace(temporary_output, output)
        elif exit_code == 0:
            exit_code = 1
        log_text = "[stdout]\n" + "".join(stdout_parts) + "\n[stderr]\n" + "".join(stderr_parts)
        log_path.write_text(log_text, encoding="utf-8")
        receipt = {
            "command": command,
            "version_command": version_command,
            "rustc_version": (version_result.stdout or version_result.stderr).strip(),
            "target": TARGET,
            "inputs_sha256_before": before,
            "inputs_sha256_after": after,
            "changed_inputs": changed,
            "stdout": "".join(stdout_parts),
            "stderr": "".join(stderr_parts),
            "exit_code": exit_code,
            "log": str(log_path),
            "log_sha256": sha256(log_path),
            "output": str(output) if output.is_file() and exit_code == 0 else None,
            "output_sha256": sha256(output) if output.is_file() and exit_code == 0 else None,
            "map": str(map_path) if map_path.is_file() else None,
            "map_sha256": sha256(map_path) if map_path.is_file() else None,
            "runtime_status": "pending",
        }
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if exit_code:
            print(f"Rust startup proxy build failed; see {log_path}", file=sys.stderr)
        return exit_code
    finally:
        if temporary_output.exists():
            temporary_output.unlink()


if __name__ == "__main__":
    raise SystemExit(main())
