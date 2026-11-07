#!/usr/bin/env python3
"""Filter rustc's PE export DEF against the pinned native ABI contract."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def entries(contents: str) -> dict[str, str]:
    lines = contents.splitlines()
    if not lines or lines[0].strip() != "EXPORTS":
        raise ValueError("DEF lacks EXPORTS header")
    result = {}
    for line in lines[1:]:
        if not line.strip() or line.lstrip().startswith(";"):
            continue
        name = line.split()[0].strip('"')
        if name in result:
            raise ValueError(f"duplicate export: {name}")
        result[name] = line
    return result


def validate_contract(data: bytes) -> dict[str, str]:
    canonical = entries(data.decode("utf-8"))
    # Freeze the original ABI byte-for-byte; the optional private Bink runtime
    # adds only these reviewed aliases, including the legacy volume name whose
    # FleetOpsHook adapter calls a three-argument (ret 12) implementation.
    extensions = {"A2FO_InitializeNoUnwind": "A2FO_InitializeNoUnwind @198",
                  "A2FO_BinkEarlyAttachNoUnwind": "A2FO_BinkEarlyAttachNoUnwind @199"}
    functions = [("Open",8,"open",8),("Close",4,"close",4),
                 ("DoFrame",4,"do_frame",4),("NextFrame",4,"next_frame",4),
                 ("Wait",4,"wait",4),("CopyToBuffer",28,"copy",28),
                 ("Goto",12,"goto",12),("SetVolume",8,"set_volume",12),
                 ("SetSoundSystem",8,"set_sound_system",8),
                 ("OpenDirectSound",4,"open_direct_sound",4),("GetError",0,"get_error",0)]
    for ordinal, (name, label_bytes, body, body_bytes) in enumerate(functions,200):
        label = f"_Bink{name}@{label_bytes}"
        extensions[label] = f'"{label}" = a2fo_bink_{body}@{body_bytes} @{ordinal}'
    primary_extensions = set(extensions)
    extensions["_BinkDDSurfaceType@4"] = '"_BinkDDSurfaceType@4" = a2fo_bink_dd_surface_type@4 @211'
    extensions["_BinkDDSurfaceTypeLegacy@4"] = '"_BinkDDSurfaceTypeLegacy@4" = a2fo_bink_dd_surface_type_legacy@4 @212'
    baseline = b"".join(line for line in data.splitlines(keepends=True)
                         if line.split() and line.split()[0].decode().strip('"') not in extensions)
    if sha256(baseline) != "b924ed582a6435af9767f3dbdc4ec665d25a4345a9369a145596ffd8d5855936":
        raise ValueError("the pinned 197-export ABI contract changed")
    if len(canonical) not in (197, 198, 210, 212):
        raise ValueError(f"expected the 197, 198, private 210 or experimental 212-export ABI, got {len(canonical)}")
    for name, expected in extensions.items():
        if name in canonical and canonical[name].split() != expected.split():
            raise ValueError(f"unapproved ABI extension: {name}")
    if len(canonical)==210 and set(canonical)&set(extensions)!=primary_extensions:
        raise ValueError("incomplete private Bink ABI")
    if len(canonical)==212 and not set(extensions)<=canonical.keys():
        raise ValueError("incomplete experimental surface ABI")
    if len(canonical)==198 and "A2FO_InitializeNoUnwind" not in canonical:
        raise ValueError("ordinal 198 must be the no-unwind initialization boundary")
    return canonical


def main() -> int:
    arguments = sys.argv[1:]
    contract = Path(os.environ["A2FO_DLL_EXPORT_CONTRACT"])
    record_dir = Path(os.environ["A2FO_DLL_LINK_RECORD_DIR"])
    record_dir.mkdir(parents=True, exist_ok=True)
    contract_data = contract.read_bytes()
    canonical = validate_contract(contract_data)
    def_args = [arg for arg in arguments if arg.startswith("-Wl,") and arg.endswith(".def")]
    if len(def_args) != 1:
        raise ValueError(f"expected one rustc DEF argument, got {def_args!r}")
    generated = Path(def_args[0][4:])
    original = generated.read_bytes()
    provided = entries(original.decode("utf-8"))
    kept = [line for name, line in provided.items() if name in canonical]
    filtered = ("EXPORTS\n" + "\n".join(kept) + ("\n" if kept else "")).encode("utf-8")
    (record_dir / "rustc-original.def").write_bytes(original)
    (record_dir / "rustc-filtered.def").write_bytes(filtered)
    generated.write_bytes(filtered)
    receipt = {
        "driver": "i686-w64-mingw32-g++",
        "argv": arguments,
        "canonical_contract": str(contract),
        "canonical_sha256": sha256(contract_data),
        "original_sha256": sha256(original),
        "filtered_sha256": sha256(filtered),
        "original_count": len(provided),
        "filtered_count": len(kept),
        "suppressed": sorted(set(provided) - set(canonical)),
    }
    result = subprocess.run(["i686-w64-mingw32-g++", *arguments], check=False)
    receipt["exit_code"] = result.returncode
    (record_dir / "linker-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return result.returncode


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"rust_dll_linker: {error}", file=sys.stderr)
        raise SystemExit(2)
