#!/usr/bin/env python3
"""Validate retired native units, direct Rust consumers and public PE contracts."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_rust_media_contract import Image
ROOT = Path(__file__).resolve().parents[1]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', required=True, type=Path)
    parser.add_argument('--baseline', type=Path, default=ROOT/'build')
    parser.add_argument('--receipt', required=True, type=Path)
    args = parser.parse_args()
    ledger = json.loads((ROOT/'rust/retired_native_adapters.json').read_text())
    make = (ROOT/'Makefile').read_text()
    for name, sha in ledger['sdk_contracts'].items():
        assert digest(ROOT/name) == sha, f'public contract changed: {name}'
    for name, sha in ledger['reference_hashes'].items():
        assert digest(ROOT/'tests/native_reference/adapters'/name) == sha, name
    for entry in ledger['retired']:
        for name in entry['removed']:
            assert not (ROOT/name).exists(), f'retired source reintroduced: {name}'
            assert name not in make, f'retired build dependency: {name}'
        assert (ROOT/entry['rust_owner']).is_file()
        assert (ROOT/entry['ffi']).is_file()
        for consumer in entry['consumers']:
            text = (ROOT/consumer).read_text()
            assert not re.search(r'#include\s+"(?:.*/)?(?:hook|decal_math)\.hpp"', text)
            assert not re.search(r'\b(?:a2fo::)?(?:install_inline_hook|patch_jump|patch_call|patch_bytes)\s*\(', text)
    manifest = (args.candidate/'rust-dll/native-link-manifest.txt').read_text().splitlines()
    assert not any(lib in manifest for lib in ledger['removed_link_libraries'])
    assert not any(Path(p).name in {'hook.cpp.o','decal_math.cpp.o'} for p in manifest if not p.startswith('-'))
    main = args.candidate/'A2FOExtensions.dll'
    symbols = subprocess.check_output(['i686-w64-mingw32-nm','-g',str(main)],text=True)
    for entry in ledger['retired']:
        for name in entry['symbols']:
            assert re.search(r'\b_?'+re.escape(name)+r'$', symbols, re.M), name
    assert not re.search(r'_ZN4a2fo\d+(?:patch_jump|patch_call|patch_bytes|install_inline_hook)', symbols)
    assert not re.search(r'_ZN4a2fo5decal\d+rotate_xyz', symbols)
    images = {}
    for name in ['A2FOExtensions.dll','modules/A2FORadarDiagnostics.dll']:
        current, before = Image(args.candidate/name), Image(args.baseline/name)
        assert current.exports == before.exports, f'export/ordinal change: {name}'
        assert set(current.imports) <= set(before.imports), f'added import: {name}'
        images[name] = {'sha256':digest(args.candidate/name),'exports':len(current.exports),'imports':current.imports}
    result = {'status':'passed-retirement-and-dll-contracts','retired_units':len(ledger['retired']),
              'removed_sources':sum((e['removed'] for e in ledger['retired']),[]),
              'removed_link_libraries':ledger['removed_link_libraries'],'images':images,
              'ledger_sha256':digest(ROOT/'rust/retired_native_adapters.json')}
    args.receipt.parent.mkdir(parents=True,exist_ok=True)
    args.receipt.write_text(json.dumps(result,indent=2)+'\n')
    print('Retired 2 C++ units, 4 source/header files and 3 linker libraries; DLL contracts preserved')
if __name__ == '__main__': main()
