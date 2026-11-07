#!/usr/bin/env python3
"""Compare full frozen/current Infestation native workflow traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/runtime-a-infestation-work'
BEFORE = ROOT / 'build/rust-migration-stage8/before'
UNIT = Path('modules/A2FOInfestation/module.cpp')
TEST = ROOT / 'tests/rust_stage8_a_infestation_native.cpp'
MODES = ('metadata', 'metadata-null', 'native-badalloc', 'trigger', 'hatch', 'cure', 'route',
         'nearest', 'numeric24', 'numeric53', 'numeric64',
         'disabled', 'short', 'init-signature', 'init-class-false',
         'init-weapon-throw', 'init-success')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_fixture(source, archive, exe):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
        '-Wpedantic', '-Werror', '-ffunction-sections', '-fdata-sections',
        '-Wl,--gc-sections', '-static', f'-DMODULE_SOURCE="{source}"',
        str(TEST), str(archive), '-lws2_32', '-lbcrypt', '-lole32',
        '-ladvapi32', '-o', str(exe),
    ]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=240)
    if result.returncode:
        raise RuntimeError(f'fixture compile failed: {command}\n'
                           f'{result.stderr[-6000:]!r}')


def run_wine(exe, mode, prefix):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
    )
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        result = subprocess.run(['wine', str(exe), mode], cwd=ROOT, env=env,
                                stdout=stdout, stderr=stderr, timeout=120)
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().replace(b'\r\n', b'\n')
        errors = stderr.read()
    if result.returncode:
        raise RuntimeError(f'{exe.name}/{mode} exited {result.returncode}: '
                           f'{output[-4000:]!r} {errors[-4000:]!r}')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
                        default=os.environ.get('A2FO_RUST_WIN_LIB',
                                               ROOT/'build/rust/i686-pc-windows-gnu/'
                                               'release/liba2fo_core.a'))
    parser.add_argument('--wine-prefix', type=Path,
                        default=os.environ.get('A2FO_RUST_WINEPREFIX',
                                               '/tmp/a2fo-shields-wine-prefix'))
    parser.add_argument('--receipt', type=Path, default=WORK/'parity.json')
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    frozen = BEFORE/UNIT
    pinned = json.loads((ROOT/'build/rust-migration-stage8/before-hashes.json').read_text())
    if digest(frozen) != pinned[str(UNIT)]:
        raise RuntimeError('frozen Infestation module source changed')
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        raise RuntimeError(f'missing Rust archive: {archive}')
    WORK.mkdir(parents=True, exist_ok=True)
    exes = {}
    for version in ('before', 'current'):
        source = frozen if version == 'before' else ROOT/UNIT
        exe = WORK/f'infestation-{version}.exe'
        compile_fixture(source, archive, exe)
        exes[version] = exe
    if args.build_only:
        print('Infestation frozen/current full native module fixtures built')
        return
    results = {}
    for mode in MODES:
        for version in ('before', 'current'):
            output = run_wine(exes[version], mode, args.wine_prefix)
            (WORK/f'{mode}-{version}.txt').write_bytes(output)
            results[(mode, version)] = output
        if results[(mode, 'before')] != results[(mode, 'current')]:
            raise RuntimeError(f'Infestation {mode} frozen/current trace mismatch')
    receipt = {
        'unit': str(UNIT),
        'cases': len(MODES),
        'lines': sum(len(results[(mode, 'before')].splitlines()) for mode in MODES),
        'frozen_sha256': digest(frozen),
        'current_sha256': digest(ROOT/UNIT),
        'rust_sha256': digest(ROOT/'rust/a2fo_core/src/stage8_a_infestation.rs'),
        'ffi_sha256': digest(ROOT/'rust/stage8_a_infestation_ffi.h'),
        'shared_text_sha256': digest(ROOT/'rust/a2fo_core/src/stage8_a_breeders.rs'),
        'fixture_sha256': digest(TEST),
        'runner_sha256': digest(Path(__file__)),
        'rust_archive': str(archive),
        'rust_archive_sha256': digest(archive),
        'equal': True,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2)+'\n')
    print(f'Infestation: {receipt["cases"]} frozen/current native traces PASS')


if __name__ == '__main__':
    main()
