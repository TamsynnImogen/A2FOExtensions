#!/usr/bin/env python3
"""Compile and compare frozen/current real Graphics Options native fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/renderer-options-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/core/renderer_options.cpp'
SOURCE = ROOT / 'core/renderer_options.cpp'
FIXTURE = ROOT / 'tests/renderer_options_test.cpp'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=180):
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                stderr=err, timeout=timeout)
        out.seek(0); err.seek(0)
        stdout, stderr = out.read(), err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{stdout[-2000:]!r}\n{stderr[-4000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def fixture_for(source, label):
    path = WORK / f'{label}_renderer_options_fixture.cpp'
    original = FIXTURE.read_text()
    marker = '#include "../core/renderer_options.cpp"'
    if original.count(marker) != 1:
        raise RuntimeError('renderer fixture include changed')
    path.write_text(original.replace(marker, f'#include "{source}"'))
    return path

def compile_fixture(source, label, archive):
    target = WORK / f'{label}_renderer_options.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
               '-Wextra', '-Wpedantic', '-Werror', '-DNDEBUG', '-DNOMINMAX',
               '-DWIN32_LEAN_AND_MEAN', '-static', '-static-libgcc',
               '-static-libstdc++', '-I', str(ROOT / 'core'),
               str(fixture_for(source, label)), str(archive),
               '-lcomctl32', '-lshlwapi', '-lole32', '-lws2_32', '-lntdll',
               '-luserenv', '-lbcrypt', '-ladvapi32', '-o', str(target)]
    run(command, timeout=300)
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--fleetops', type=Path, default=ROOT / 'FleetOpsHook.dll')
    args = parser.parse_args()
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    if sha(FROZEN) != HASHES['core/renderer_options.cpp']:
        raise RuntimeError('frozen renderer source hash changed')
    for path in (archive, args.fleetops):
        if not path.is_file():
            raise RuntimeError(f'required input missing: {path}')
    archive_hash = sha(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    executables = [compile_fixture(FROZEN, 'frozen', archive),
                   compile_fixture(SOURCE, 'current', archive)]
    if args.compile_only:
        print('renderer options: frozen/current GNU++17/O2/Werror fixture compile PASS; Wine skipped')
        return
    prefix = args.wine_prefix or Path(os.environ.get('A2FO_RUST_WINEPREFIX',
        WORK / 'wine-prefix'))
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        raise RuntimeError('initialized absolute disposable Wine prefix required')
    env = os.environ.copy()
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', DISPLAY='',
               WAYLAND_DISPLAY='',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    transcripts = []
    for executable in executables:
        modes = []
        for layout in ('clean', 'roots'):
            command = ['wine', str(executable), str(args.fleetops.resolve()),
                       str(args.fleetops.resolve())]
            if layout == 'clean':
                command.append('--clean-layout')
            output = run(command, env=env, timeout=120)
            if not re.fullmatch(rb'renderer options: \d+x\d+ layout, signatures, foreign guard, HWND, effect lifecycle and persistence passed\n', output):
                raise RuntimeError(f'unexpected {layout} transcript: {output!r}')
            modes.append(output)
        transcripts.append(b''.join(modes))
    if transcripts[0] != transcripts[1]:
        raise RuntimeError('frozen/current renderer options transcripts differ')
    if sha(archive) != archive_hash:
        raise RuntimeError('Rust archive changed during comparison')
    receipt = {'frozen_source_sha256': sha(FROZEN),
               'current_source_sha256': sha(SOURCE),
               'rust_archive_sha256': archive_hash,
               'fixture_sha256': sha(FIXTURE),
               'compiler': 'GNU++17/O2/Werror', 'cases': 2,
               'transcript_sha256': hashlib.sha256(transcripts[0]).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('renderer options: 2 frozen/current native layouts PASS')

if __name__ == '__main__':
    main()
