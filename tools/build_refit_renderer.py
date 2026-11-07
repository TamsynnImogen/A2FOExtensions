#!/usr/bin/env python3
"""Build and stage Refit Windows DLLs without touching a game installation."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tomllib


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "third_party/armada2-refit-native"
MANIFEST = SOURCE / "UPSTREAM-MANIFEST.json"
TARGET = "i686-pc-windows-msvc"
COMPONENTS = {
    "display": (SOURCE / "display", "display.dll", "A2FORefitDisplay.dll", "a2fo-host"),
    "renderer": (SOURCE / "render3d", "render3d.dll", "A2FORefitRenderer.dll", "a2fo-host"),
}


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def provenance():
    if not MANIFEST.is_file():
        raise RuntimeError(f"missing pinned source manifest: {MANIFEST}")
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("commit") != "1e2a7d2fc213282ef7f9ef00e1b11011da23ac68":
        raise RuntimeError("unexpected pinned Refit source commit")
    baseline = manifest.get("files")
    if not isinstance(baseline, dict) or len(baseline) != 1652:
        raise RuntimeError("incomplete pinned Refit source manifest")
    actual = {}
    for path in SOURCE.rglob("*"):
        if path.is_file() and not path.is_symlink() and not any(part in {"target", ".git"} for part in path.relative_to(SOURCE).parts):
            actual[path.relative_to(SOURCE).as_posix()] = digest(path)
    missing = sorted(set(baseline) - set(actual))
    changed = sorted(name for name in baseline.keys() & actual.keys() if actual[name] != baseline[name]["sha256"])
    added = sorted(set(actual) - set(baseline) - {"UPSTREAM-MANIFEST.json"})
    return {
        "manifest": str(MANIFEST),
        "manifest_sha256": digest(MANIFEST),
        "upstream_commit": manifest["commit"],
        "upstream_archive_sha256": manifest.get("archive_sha256"),
        "upstream_file_count": len(baseline),
        "actual_file_sha256": actual,
        "local_changes": {"changed": changed, "added": added, "missing": missing},
        "local_patch_sha256": {name: actual[name] for name in changed + added},
    }


def feature_ready(directory, feature):
    if feature is None:
        return True
    cargo = tomllib.loads((directory / "Cargo.toml").read_text(encoding="utf-8"))
    return feature in cargo.get("features", {})


def component_hashes(directory):
    return {
        path.relative_to(directory).as_posix(): digest(path)
        for path in directory.rglob("*")
        if path.is_file() and not path.is_symlink() and "target" not in path.relative_to(directory).parts
    }


def write_linker_wrapper(wrapper, linker):
    quoted = shlex.quote(str(linker))
    wrapper.parent.mkdir(parents=True, exist_ok=True)
    wrapper.write_text(
        "#!/bin/sh\n"
        'if [ "$1" = "-flavor" ] && [ "$2" = "link" ]; then\n'
        f"    exec {quoted} \"$@\"\n"
        "fi\n"
        f"exec {quoted} -flavor link \"$@\"\n",
        encoding="utf-8",
    )
    wrapper.chmod(0o755)


def run(argv, cwd, env=None):
    result = subprocess.run(argv, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    return {"argv": argv, "cwd": str(cwd), "exit_code": result.returncode, "output": result.stdout}


def valid_pe_dll_tables(output):
    if "file format pei-i386" not in output or "The Export Tables" not in output:
        return False
    parts = output.split("Characteristics ", 1)
    if len(parts) != 2:
        return False
    characteristics = parts[1].split("\n\n", 1)[0]
    return bool(re.search(r"(?m)^[ \t]+DLL[ \t]*$", characteristics))


def build(component, output_dir, toolchain="1.96.0", offline=False):
    directory, built_name, stage_name, feature = COMPONENTS[component]
    output_dir = output_dir.resolve()
    stage = output_dir / stage_name
    receipt_path = output_dir / f"{component}-receipt.json"
    output_dir.mkdir(parents=True, exist_ok=True)
    stage.unlink(missing_ok=True)
    receipt_path.unlink(missing_ok=True)
    receipt = {"component": component, "stage_only": True, "status": "failed", "commands": []}
    try:
        source = provenance()
        receipt["source"] = source
        cargo_toml = directory / "Cargo.toml"
        if not cargo_toml.is_file() or not (directory / "Cargo.lock").is_file():
            raise RuntimeError(f"missing Cargo.toml or Cargo.lock: {directory}")
        if not feature_ready(directory, feature):
            raise RuntimeError(f"{directory}/Cargo.toml lacks required feature {feature}; host adaptation is not ready")
        receipt["component_source_sha256"] = component_hashes(directory)
        for tool in ("cargo", "llvm-dlltool", "llvm-rc", "i686-w64-mingw32-objdump"):
            if shutil.which(tool) is None:
                raise RuntimeError(f"required tool missing: {tool}")
        rustup_home = Path(os.environ.get("RUSTUP_HOME", Path.home() / ".rustup"))
        toolchain_dir = rustup_home / "toolchains" / toolchain
        if not toolchain_dir.is_dir() and "-" not in toolchain:
            toolchain_dir = rustup_home / "toolchains" / f"{toolchain}-x86_64-unknown-linux-gnu"
        linker = toolchain_dir / "lib/rustlib/x86_64-unknown-linux-gnu/bin/rust-lld"
        if not linker.is_file():
            raise RuntimeError(f"missing Rust bundled linker: {linker}")
        wrapper = output_dir / "tools/lld-link"
        write_linker_wrapper(wrapper, linker)
        target_dir = output_dir / "target" / component
        built = target_dir / TARGET / "release" / built_name
        env = os.environ.copy()
        env["CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER"] = str(wrapper)
        env["CARGO_TARGET_DIR"] = str(target_dir)
        receipt["environment_overrides"] = {key: env[key] for key in ("CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER", "CARGO_TARGET_DIR")}
        command = ["cargo", f"+{toolchain}", "build", "--locked", "--release", "--target", TARGET, "--target-dir", str(target_dir)]
        if offline:
            command.append("--offline")
        if feature:
            command.extend(["--features", feature])
        result = run(command, directory, env)
        receipt["commands"].append(result)
        if result["exit_code"]:
            raise RuntimeError(f"Cargo build failed with exit {result['exit_code']}")
        if not built.is_file():
            raise RuntimeError(f"Cargo succeeded without expected DLL: {built}")
        if provenance()["actual_file_sha256"] != source["actual_file_sha256"]:
            raise RuntimeError("vendored source changed during build")
        if component_hashes(directory) != receipt["component_source_sha256"]:
            raise RuntimeError("component source changed during build")
        inspected = run(["i686-w64-mingw32-objdump", "-p", str(built)], ROOT)
        receipt["commands"].append(inspected)
        if inspected["exit_code"] or not valid_pe_dll_tables(inspected["output"]):
            raise RuntimeError("DLL import/export inspection failed")
        receipt["binary_tables"] = inspected["output"]
        shutil.copyfile(built, stage)
        receipt["artifact"] = {"cargo_path": str(built), "staged_path": str(stage), "sha256": digest(stage)}
        receipt["status"] = "staged"
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
        stage.unlink(missing_ok=True)
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return receipt["status"] == "staged", receipt_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--component", choices=COMPONENTS, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/refit")
    parser.add_argument("--toolchain", default="1.96.0")
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    success, receipt = build(args.component, args.output_dir, args.toolchain, args.offline)
    print(f"{'staged' if success else 'failed'}: {receipt}")
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
