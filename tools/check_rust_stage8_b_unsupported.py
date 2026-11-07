#!/usr/bin/env python3
"""Frozen/current native diagnostic parity for D3D8 unsupported methods."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BEFORE = ROOT / 'build/rust-migration-stage8/before'
WORK = ROOT / 'build/rust-migration-stage8/unsupported-work'
SOURCE = Path('renderer/d3d8_compat/game/unsupported.cpp')
FIXTURE = ROOT / 'tests/rust_stage8_b_unsupported.cpp'
PRIVATE = ROOT / 'tests/rust_stage8_b_unsupported_private.hpp'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=90):
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        done = subprocess.run(command, cwd=ROOT, env=env, stdout=output,
                              stderr=errors, timeout=timeout)
        output.seek(0); errors.seek(0)
        stdout, stderr = output.read(), errors.read()
    if done.returncode:
        raise RuntimeError(f'{command}: exit {done.returncode}\n{stdout[-2000:]!r}\n{stderr[-3000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path)
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    if sha(BEFORE / SOURCE) != HASHES[str(SOURCE)]:
        raise RuntimeError('frozen source hash mismatch')
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    if not archive.is_file():
        raise RuntimeError(f'Windows Rust archive missing: {archive}')
    archive_hash = sha(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    exes = []
    for label, source in (('before', BEFORE / SOURCE), ('current', ROOT / SOURCE)):
        text = source.read_text().replace('#include "private.hpp"',
            f'#include "{PRIVATE}"', 1)
        if label == 'current':
            text = text.replace('#include "../../../rust/stage8_b_unsupported_ffi.h"',
                f'#include "{ROOT / "rust/stage8_b_unsupported_ffi.h"}"', 1)
        generated = WORK / f'{label}.cpp'
        generated.write_text(text)
        exe = WORK / f'{label}.exe'
        command = ['i686-w64-mingw32-g++', '-DWIN32_LEAN_AND_MEAN',
            '-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-static', str(FIXTURE), str(generated)]
        if label == 'current': command.append(str(archive))
        command += ['-o', str(exe)]
        run(command, timeout=180)
        exes.append(exe)
    if args.compile_only:
        print('unsupported: frozen/current GNU++17/O2 native mocks compile; Wine skipped')
        return
    prefix = args.wine_prefix or Path(os.environ.get('A2FO_RUST_WINEPREFIX',
        WORK / 'wine-prefix'))
    env = os.environ.copy()
    env.update(WINEPREFIX=str(prefix.resolve()), WINEDEBUG='-all', DISPLAY='',
               WAYLAND_DISPLAY='', WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    before, current = [run(['wine', str(exe)], env=env) for exe in exes]
    if before != current:
        for index, (old, new) in enumerate(zip(before.splitlines(), current.splitlines()), 1):
            if old != new:
                raise RuntimeError(f'unsupported line {index}: {old!r} != {new!r}')
        raise RuntimeError('unsupported transcript length differs')
    if sha(archive) != archive_hash:
        raise RuntimeError('Windows Rust archive changed during comparison')
    receipt = {'source_before_sha256': sha(BEFORE / SOURCE),
        'source_current_sha256': sha(ROOT / SOURCE), 'fixture_sha256': sha(FIXTURE),
        'rust_archive_sha256': archive_hash, 'compiler': 'GNU++17/O2/Werror',
        'cases': len(before.splitlines()), 'transcript_sha256': hashlib.sha256(before).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f"unsupported: {receipt['cases']} frozen/current native traces, PASS")

if __name__ == '__main__': main()
