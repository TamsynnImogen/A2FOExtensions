#!/usr/bin/env python3
"""Link the existing native object set into the Rust-owned DLL root."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "rust/a2fo_dll/Cargo.toml"
LINKER_WRAPPER = ROOT / "tools/rust_dll_linker.py"
TARGET = "i686-pc-windows-gnu"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--bink-runtime", action="store_true", help="private Rust Bink evaluation build; requires the paired startup proxy")
    parser.add_argument("--bink-surface-runtime", action="store_true", help="experimental private Bink build with explicit current/legacy surface classifiers")
    args = parser.parse_args()
    bink_runtime = args.bink_runtime or args.bink_surface_runtime
    output = args.output.resolve()
    objects = shlex.split(os.environ["A2FO_DLL_NATIVE_OBJECTS"])
    libraries = shlex.split(os.environ["A2FO_DLL_NATIVE_LIBS"])
    if not objects:
        parser.error("A2FO_DLL_NATIVE_OBJECTS is empty")
    if any(not library.startswith(("-l", "-L")) for library in libraries):
        parser.error("A2FO_DLL_NATIVE_LIBS only accepts -l and -L arguments")
    native = [Path(name).resolve(strict=True) for name in objects]
    if len(set(native)) != len(native):
        parser.error("duplicate native object")
    if any(not path.is_file() for path in native):
        parser.error("all native object inputs must be regular files")
    if any("\n" in value for value in [*map(str, native), *libraries]):
        parser.error("newline in link argument")
    output.parent.mkdir(parents=True, exist_ok=True)
    build_dir = output.parent / "rust-dll"
    build_dir.mkdir(exist_ok=True)
    manifest = build_dir / "native-link-manifest.txt"
    arguments = [*map(str, native), *libraries]
    manifest.write_text("\n".join(arguments) + "\n", encoding="utf-8")
    command = ["cargo", "build", "--offline", "--locked", "--manifest-path", str(MANIFEST), "--target-dir", str(build_dir), "--target", TARGET]
    command += ["--profile", "bink", "--features", "bink-runtime"] if bink_runtime else ["--release"]
    env = os.environ.copy()
    env["A2FO_DLL_LINK_MANIFEST"] = str(manifest)
    env[f"CARGO_TARGET_{TARGET.upper().replace('-', '_')}_LINKER"] = str(LINKER_WRAPPER)
    env["A2FO_DLL_LINK_RECORD_DIR"] = str(build_dir)
    contract = "core/A2FOExtensions.bink-surface.def" if args.bink_surface_runtime else "core/A2FOExtensions.bink.def" if bink_runtime else "core/A2FOExtensions.def"
    env["A2FO_DLL_EXPORT_CONTRACT"] = str(ROOT / contract)
    # The previous native DLL link uses the GCC static runtime and stdlib.
    env["RUSTFLAGS"] = " ".join((env.get("RUSTFLAGS", ""), "-C link-arg=-static", "-C link-arg=-static-libgcc", "-C link-arg=-static-libstdc++", "-C link-arg=-Wl,--enable-stdcall-fixup")).strip()
    extra_inputs = []
    for folder in [ROOT/"rust/a2fo_native11_model",ROOT/"third_party/armada2-refit/menu-renderer/a2fmt"]:
        extra_inputs += [p for p in folder.rglob("*") if p.is_file() and "target" not in p.relative_to(folder).parts and p.suffix in (".rs",".toml",".lock")]
    if bink_runtime:
        for folder in [ROOT/"rust/a2fo_bink_runtime", ROOT/"rust/a2fo_bink_candidate", ROOT/"third_party/rust-bink-evaluation/infinitier_bik_decoder"]:
            extra_inputs += [p for p in folder.rglob("*") if p.is_file() and "target" not in p.relative_to(folder).parts and p.suffix in (".rs",".toml",".lock",".def")]
    receipt = {
        "bink_runtime": bink_runtime,
        "bink_surface_runtime": args.bink_surface_runtime,
        "command": command,
        "linker": env[f"CARGO_TARGET_{TARGET.upper().replace('-', '_')}_LINKER"],
        "rustflags": env["RUSTFLAGS"],
        "export_contract": env["A2FO_DLL_EXPORT_CONTRACT"],
        "export_contract_sha256": sha256(Path(env["A2FO_DLL_EXPORT_CONTRACT"])),
        "manifest": str(manifest),
        "manifest_sha256": sha256(manifest),
        "inputs": {str(path): sha256(path) for path in [
            *native, MANIFEST, MANIFEST.parent / "Cargo.lock",
            MANIFEST.parent / "build.rs", *sorted((MANIFEST.parent / "src").rglob("*.rs")),
            ROOT / "rust/a2fo_core/Cargo.toml", ROOT / "rust/a2fo_core/Cargo.lock",
            *sorted((ROOT / "rust/a2fo_core/src").rglob("*.rs")),
            LINKER_WRAPPER, Path(__file__).resolve(), Path(env["A2FO_DLL_EXPORT_CONTRACT"]), *extra_inputs,
        ]},
    }
    log = build_dir / "cargo-build.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT, check=False)
    receipt["inputs_after"] = {name: sha256(Path(name)) for name in receipt["inputs"]}
    receipt["changed_inputs"] = [name for name, digest in receipt["inputs"].items() if receipt["inputs_after"][name] != digest]
    receipt["exit_code"] = result.returncode or (1 if receipt["changed_inputs"] else 0)
    receipt["log"] = str(log)
    receipt["log_sha256"] = sha256(log)
    built = build_dir / TARGET / ("bink" if bink_runtime else "release") / "a2fo_dll.dll"
    if receipt["exit_code"] == 0 and built.is_file():
        shutil.copy2(built, output)
        receipt["output"] = str(output)
        receipt["output_sha256"] = sha256(output)
    else:
        receipt["exit_code"] = receipt["exit_code"] or 1
    (build_dir / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if receipt["exit_code"]:
        print(f"Rust DLL build failed; see {log}", file=sys.stderr)
    return receipt["exit_code"]


if __name__ == "__main__":
    raise SystemExit(main())
