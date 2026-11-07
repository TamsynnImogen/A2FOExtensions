#!/usr/bin/env python3
"""Compare frozen/current Mods menu in a headless native mock executable."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/core-menu-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/core/module_menu.cpp'
SOURCE = ROOT / 'core/module_menu.cpp'
FIXTURE = ROOT / 'tests/rust_stage8_b_core_native.cpp'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())
MODES = ('missing', 'signature', 'install', 'selection', 'launch', 'failed-launch')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=180):
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                    stderr=err, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            out.seek(0); err.seek(0)
            raise RuntimeError(f'{command}: timed out after {timeout}s\n'
                               f'stdout={out.read()[-2000:]!r}\n'
                               f'stderr={err.read()[-4000:]!r}') from exc
        out.seek(0); err.seek(0)
        stdout, stderr = out.read(), err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{stdout[-2000:]!r}\n{stderr[-4000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def prepare_fixture(source, label):
    fixture = WORK / f'{label}_core_menu_fixture.cpp'
    original = FIXTURE.read_text()
    marker = '#include "../core/module_menu.cpp"'
    if original.count(marker) != 1:
        raise RuntimeError('core native fixture include changed')
    fixture.write_text(original.replace(marker, f'#include "{source}"'))
    return fixture

def compile_fixture(source, label, archive):
    target = WORK / f'{label}_core_menu.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
               '-Wextra', '-Wpedantic', '-Werror',
               # These diagnostics are present in the pinned frozen source.
               '-Wno-cast-function-type', '-Wno-unused-function',
               '-DNDEBUG', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
               '-static', '-static-libgcc', '-static-libstdc++',
               '-I', str(ROOT / 'core'), str(prepare_fixture(source, label)),
               str(archive), '-lcomctl32', '-lgdi32', '-lole32', '-lshell32',
               '-lshlwapi', '-lws2_32', '-lntdll', '-luserenv', '-lbcrypt',
               '-ladvapi32', '-o', str(target)]
    run(command, timeout=300)
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path)
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    if sha(FROZEN) != HASHES['core/module_menu.cpp']:
        raise RuntimeError('frozen module menu hash changed')
    if not archive.is_file():
        raise RuntimeError(f'Rust archive missing: {archive}')
    archive_hash = sha(archive)
    input_hashes = {path: sha(path) for path in (FROZEN, SOURCE, FIXTURE, archive)}
    def check_input_hashes():
        for path, initial in input_hashes.items():
            if sha(path) != initial:
                raise RuntimeError(f'input changed during core menu comparison: {path}')
    WORK.mkdir(parents=True, exist_ok=True)
    executables = [compile_fixture(FROZEN, 'frozen', archive),
                   compile_fixture(SOURCE, 'current', archive)]
    check_input_hashes()
    if args.compile_only:
        print('core menu: frozen/current GNU++17/O2/Werror native fixture compile PASS; Wine skipped')
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
        outputs = []
        for mode in MODES:
            output = run(['wine', str(executable), mode], env=env, timeout=120)
            check_input_hashes()
            if not re.fullmatch(rb'core module menu ' + mode.encode() + rb': PASS\n', output):
                raise RuntimeError(f'unexpected {mode} transcript: {output!r}')
            outputs.append(output)
        transcripts.append(b''.join(outputs))
    if transcripts[0] != transcripts[1]:
        raise RuntimeError('frozen/current core menu transcripts differ')
    check_input_hashes()
    receipt = {'frozen_source_sha256': sha(FROZEN),
               'current_source_sha256': sha(SOURCE),
               'rust_archive_sha256': archive_hash,
               'fixture_sha256': sha(FIXTURE),
               'compiler': 'GNU++17/O2/Werror with two frozen warning suppressions',
               'cases': len(MODES),
               'transcript_sha256': hashlib.sha256(transcripts[0]).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'core menu: {len(MODES)} frozen/current headless native cases PASS')

if __name__ == '__main__':
    main()
