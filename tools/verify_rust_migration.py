#!/usr/bin/env python3
"""Check a migrated PE DLL against a frozen public export contract."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def inspect(path):
    output = subprocess.check_output(
        ["i686-w64-mingw32-objdump", "-p", str(path)], text=True)
    if "file format pei-i386" not in output:
        raise ValueError(f"Not a 32-bit Windows DLL: {path}")
    base = int(re.search(r"Ordinal Base\s+(\d+)", output).group(1))
    table = output.split("[Ordinal/Name Pointer] Table", 1)[1].split("\n\n", 1)[0]
    exports = {name: base + int(index) for index, name in re.findall(
        r"^\s*\[\s*(\d+)\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?(\w+)\s*$",
        table, re.MULTILINE)}
    if not exports:
        raise ValueError(f"No exports found: {path}")
    imports = sorted(set(re.findall(r"DLL Name:\s*(\S+)", output)))
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "exports": exports, "imports": imports}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--dll", type=Path, default=Path("build/A2FOExtensions.dll"))
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--require-symbol", action="append", default=[],
                        help="additional private Rust implementation symbol required in this stage")
    args = parser.parse_args()
    before, after = inspect(args.baseline), inspect(args.dll)
    if before["exports"] != after["exports"]:
        raise SystemExit("FAIL: public export names or ordinals changed")
    symbols = subprocess.check_output(
        ["i686-w64-mingw32-nm", "--defined-only", str(args.dll)], text=True)
    required = ["a2fo_rs_builtin_request", "a2fo_rs_policy_evaluate", "a2fo_rs_resupply_parse", *args.require_symbol]
    for name in required:
        if not re.search(r"\b[Tt]\s+_?" + name + r"$", symbols, re.MULTILINE):
            raise SystemExit(f"FAIL: Rust implementation not linked: {name}")
    subprocess.run(["python3", "tools/verify_builtin_modules.py", str(args.dll)], check=True)
    receipt = {"baseline": before, "migrated": after,
               "export_names_and_ordinals_equal": True,
               "new_imports": sorted(set(after["imports"]) - set(before["imports"])),
               "rust_implementations_linked": True,
               "required_implementation_symbols": required}
    if receipt["new_imports"]:
        raise SystemExit("FAIL: migration adds system DLL dependencies: " +
                         ", ".join(receipt["new_imports"]))
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"Rust DLL boundary: {len(after['exports'])} unchanged exports/ordinals; PASS")
    print("New system imports: " + (", ".join(receipt["new_imports"]) or "none"))


if __name__ == "__main__":
    main()
