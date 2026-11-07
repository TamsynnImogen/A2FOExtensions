#!/usr/bin/env python3
"""Compare frozen/current full M3D native module traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/runtime-a-m3d-work'
BEFORE = ROOT / 'build/rust-migration-stage8/before'
UNIT = Path('modules/A2FOM3D/module.cpp')
TEST = ROOT / 'tests/rust_stage8_a_m3d_native.cpp'
MODES = (
    'init-short', 'init-no-armada', 'init-armada-throw',
    'init-roots-throw', 'init-signature', 'init-hook-first',
    'init-hook-second', 'init-hook-throw', 'init-success',
    'cached', 'cached-no-fallback', 'cached-invalid',
    'resolver-off', 'resolver-null', 'resolver-missing', 'load-fail',
    'hierarchy', 'names',
)
FLAVORS = ('standalone', 'builtin')
EXPECTED_EXCEPTION = {'init-armada-throw', 'init-roots-throw', 'init-hook-throw'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_fixture(source, archive, exe, flavor):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
        '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source}"',
    ]
    if flavor == 'builtin':
        command.append('-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::m3d')
    command += [
        str(TEST), str(ROOT/'modules/A2FOM3D/m3d_conversion.cpp'),
        str(archive), '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32',
        '-o', str(exe),
    ]
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=240)
    if result.returncode:
        raise RuntimeError(f'fixture compile failed: {command}\n'
                           f'{result.stderr[-7000:]!r}')


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
                                stdout=stdout, stderr=stderr, timeout=180)
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().replace(b'\r\n', b'\n')
        errors = stderr.read().replace(b'\r\n', b'\n')
    if result.returncode and mode not in EXPECTED_EXCEPTION:
        raise RuntimeError(f'{exe.name}/{mode} exited {result.returncode}: '
                           f'{output[-4000:]!r} {errors[-4000:]!r}')
    if result.returncode == 0 and mode in EXPECTED_EXCEPTION:
        raise RuntimeError(f'{exe.name}/{mode} did not propagate native exception')
    return (f'exit:{result.returncode}\n'.encode() + b'stdout:\n' + output +
            b'stderr:\n' + errors)


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
        raise RuntimeError('frozen M3D module source changed')
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        raise RuntimeError(f'missing Rust archive: {archive}')
    WORK.mkdir(parents=True, exist_ok=True)
    exes = {}
    for flavor in FLAVORS:
        for version in ('before', 'current'):
            source = frozen if version == 'before' else ROOT/UNIT
            exe = WORK/f'm3d-{flavor}-{version}.exe'
            compile_fixture(source, archive, exe, flavor)
            exes[(flavor, version)] = exe
    if args.build_only:
        print('M3D frozen/current standalone+builtin module fixtures built')
        return
    results = {}
    for flavor in FLAVORS:
        for mode in MODES:
            for version in ('before', 'current'):
                output = run_wine(exes[(flavor, version)], mode,
                                  args.wine_prefix)
                (WORK/f'{flavor}-{mode}-{version}.txt').write_bytes(output)
                results[(flavor, mode, version)] = output
            if results[(flavor, mode, 'before')] != results[(flavor, mode, 'current')]:
                raise RuntimeError(f'M3D {flavor}/{mode} frozen/current mismatch')
    receipt = {
        'unit': str(UNIT),
        'cases': len(MODES) * len(FLAVORS),
        'lines': sum(len(results[(flavor, mode, 'before')].splitlines())
                     for flavor in FLAVORS for mode in MODES),
        'frozen_sha256': digest(frozen),
        'current_sha256': digest(ROOT/UNIT),
        'rust_sha256': digest(ROOT/'rust/a2fo_core/src/stage8_a_m3d.rs'),
        'ffi_sha256': digest(ROOT/'rust/stage8_a_m3d_ffi.h'),
        'fixture_sha256': digest(TEST),
        'runner_sha256': digest(Path(__file__)),
        'rust_archive': str(archive),
        'rust_archive_sha256': digest(archive),
        'equal': True,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'M3D: {receipt["cases"]} frozen/current native traces PASS')


if __name__ == '__main__':
    main()
