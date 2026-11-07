#!/usr/bin/env python3
"""Frozen/current full-source native differential for faction rendering."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/texture-runtime-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOTextureVariants/module.cpp'
CURRENT = ROOT / 'modules/A2FOTextureVariants/module.cpp'
HELPER = ROOT / 'modules/A2FOTextureVariants/texture_variants.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage8_texture_runtime.rs'
HEADER = ROOT / 'rust/stage8_texture_runtime_ffi.h'
TEST = ROOT / 'tests/rust_stage8_texture_runtime_native.cpp'
FROZEN_SHA256 = '315654de9ec77558b1d923c03a092ea07666fed379e4ae560f5728fa7dd7ef0d'
DEFAULT_ARCHIVE = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
MODES = (
    'race-valid', 'race-invalid', 'race-absent', 'race-exception',
    'hit', 'miss', 'cache', 'mutate-miss', 'borg', 'base',
    'node-show', 'node-hide', 'node-mutate', 'node-admission',
    'disabled', 'null',
)
PINS = (FROZEN, CURRENT, HELPER, RUST, HEADER, TEST)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pins(archive):
    return {str(path): digest(path) for path in (*PINS, archive)}


def build(source, archive, exe):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
        '-Wpedantic', '-Werror', '-Wno-array-bounds',
        '-Wno-mismatched-new-delete', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source}"', str(TEST), str(HELPER),
        str(archive), '-ladvapi32', '-o', str(exe),
    ]
    subprocess.run(command, cwd=ROOT, check=True, timeout=240)


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
    if result.returncode:
        raise RuntimeError(f'{exe.name}/{mode} exited {result.returncode}: '
                           f'{output[-3000:]!r} {errors[-3000:]!r}')
    if not output or b'race-name:' not in output or b'node-flags:' not in output:
        raise RuntimeError(f'{exe.name}/{mode} lacks expected state trace')
    return output


def check_exception_trace(output, version):
    if b'exception:typed\n' not in output or b'injection:used\n' not in output:
        raise RuntimeError(f'{version}: typed allocation failure not propagated')
    inside = re.search(rb'^lifetime:inside:(\d+)$', output, re.MULTILINE)
    after = re.search(rb'^lifetime:after:(\d+)$', output, re.MULTILINE)
    if not inside or not after or int(after[1]) <= int(inside[1]):
        raise RuntimeError(f'{version}: native exception lifetime trace invalid')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_ARCHIVE)))
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    '/tmp/a2fo-texture-wine-prefix')))
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    if digest(FROZEN) != FROZEN_SHA256:
        parser.error('frozen TextureVariants source drift')
    if not archive.is_file():
        parser.error(f'Rust archive missing: {archive}')
    if archive.stat().st_mtime_ns < RUST.stat().st_mtime_ns:
        parser.error('Rust archive predates texture runtime source')
    symbols = subprocess.run(
        ['i686-w64-mingw32-nm', '-g', '--defined-only', str(archive)],
        cwd=ROOT, check=True, capture_output=True, text=True).stdout
    for symbol in ('a2fo_rs_stage8_texture_race_policy',
                   'a2fo_rs_stage8_texture_apply_meshes',
                   'a2fo_rs_stage8_texture_apply_nodes'):
        if symbol not in symbols:
            parser.error(f'Rust archive missing {symbol}')
    initial = pins(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    exes = {}
    for version, source in (('frozen', FROZEN), ('current', CURRENT)):
        exe = WORK / f'texture-{version}.exe'
        build(source, archive, exe)
        if pins(archive) != initial:
            parser.error('source or archive drift during fixture build')
        exes[version] = exe
    receipt = {
        'pins': initial,
        'fixtures': {version: digest(exe) for version, exe in exes.items()},
        'build_only': args.build_only,
        'modes': [],
    }
    if not args.build_only:
        for mode in MODES:
            frozen = run_wine(exes['frozen'], mode, args.wine_prefix)
            current = run_wine(exes['current'], mode, args.wine_prefix)
            (WORK / f'{mode}-frozen.txt').write_bytes(frozen)
            (WORK / f'{mode}-current.txt').write_bytes(current)
            if mode == 'race-exception':
                check_exception_trace(frozen, 'frozen')
                check_exception_trace(current, 'current')
            if frozen != current:
                raise RuntimeError(f'{mode}: frozen/current native trace mismatch')
            if mode == 'mutate-miss' and b'mutate:original=replacement' not in current:
                raise RuntimeError('mutation fixture did not change source row')
            if mode == 'node-mutate' and b'mutate:flags=0' not in current:
                raise RuntimeError('node read-order fixture did not mutate flags')
            if pins(archive) != initial:
                parser.error('source or archive drift during fixture run')
            receipt['modes'].append({
                'name': mode, 'trace_sha256': hashlib.sha256(current).hexdigest(),
            })
    (WORK / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
