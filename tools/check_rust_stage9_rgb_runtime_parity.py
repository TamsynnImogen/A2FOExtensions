#!/usr/bin/env python3
"""Compare frozen and current RGB module source through one native fixture."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/rgb-work'
UNIT = ROOT / 'modules/A2FORGBTextures/module.cpp'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FORGBTextures/module.cpp'
TEST = ROOT / 'tests/rust_stage9_rgb_runtime_native.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_rgb_runtime.rs'
HEADER = ROOT / 'rust/stage9_rgb_runtime_ffi.h'
FROZEN_SHA256 = 'ca1413ac54bcfb88018a1b1ce451272f8e10afd71d1950fa1b5f0a922978bd1f'
DEFAULT_LIB = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_fixture(source, executable, archive=None, overlay=None):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-I.',
        '-Wall', '-Wextra', '-Wpedantic', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source.relative_to(ROOT)}"',
        str(TEST),
    ]
    if archive:
        if overlay:
            # The canonical archive's single codegen unit also defines older RGB
            # exports. Prefer the preceding overlay while retaining other exports.
            command.append('-Wl,--allow-multiple-definition')
            command.append(str(overlay))
        command += [str(archive), '-lntdll', '-lws2_32', '-luserenv', '-lbcrypt']
    command += ['-o', str(executable)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=240)
    if result.returncode:
        raise RuntimeError(f'fixture compile failed: {command}\n'
                           f'{result.stderr[-8000:]!r}')


def run(executable, prefix):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    stdout_path = WORK / f'{executable.stem}.stdout'
    stderr_path = WORK / f'{executable.stem}.stderr'
    with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
        result = subprocess.run(['wine', str(executable)], cwd=WORK,
                                env=env, stdout=stdout, stderr=stderr,
                                timeout=120)
    if result.returncode:
        raise RuntimeError(f'{executable.name} exited {result.returncode}: '
                           f'{stdout_path.read_bytes()[-4000:]!r} '
                           f'{stderr_path.read_bytes()[-4000:]!r}')
    return stdout_path.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_LIB)))
    parser.add_argument('--rgb-rlib', type=Path,
                        help='isolated RGB rlib layered before the canonical archive')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get(
                            'A2FO_RUST_WINEPREFIX', WORK / 'wine-prefix')))
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    overlay = args.rgb_rlib.resolve() if args.rgb_rlib else None
    if digest(FROZEN) != FROZEN_SHA256:
        parser.error('frozen RGB source drift')
    if not archive.is_file():
        parser.error(f'Rust archive missing: {archive}')
    if overlay and not overlay.is_file():
        parser.error(f'RGB overlay missing: {overlay}')
    inputs = (UNIT, FROZEN, TEST, RUST, HEADER, Path(__file__).resolve())
    active = overlay or archive
    if active.stat().st_mtime_ns < max(path.stat().st_mtime_ns for path in (RUST, HEADER)):
        parser.error('Rust archive predates RGB Rust source/header')
    WORK.mkdir(parents=True, exist_ok=True)
    before_hashes = {str(path.relative_to(ROOT)): digest(path) for path in inputs}
    archive_hash = digest(archive)
    overlay_hash = digest(overlay) if overlay else None
    before = WORK / 'rgb-before.exe'
    after = WORK / 'rgb-after.exe'
    compile_fixture(FROZEN, before)
    compile_fixture(UNIT, after, archive, overlay)
    if ({str(path.relative_to(ROOT)): digest(path) for path in inputs} != before_hashes
            or digest(archive) != archive_hash
            or (overlay and digest(overlay) != overlay_hash)):
        raise RuntimeError('RGB source or archive changed during fixture build')
    receipt = {
        'inputs_sha256': before_hashes,
        'archive': str(archive), 'archive_sha256': archive_hash,
        'before_sha256': digest(before), 'after_sha256': digest(after),
        'mode': 'build-only' if args.build_only else 'native-parity',
    }
    if overlay:
        receipt['overlay'] = str(overlay)
        receipt['overlay_sha256'] = overlay_hash
    if not args.build_only:
        prefix = args.wine_prefix.resolve()
        if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
            parser.error('initialized absolute private Wine prefix required')
        original = run(before, prefix)
        migrated = run(after, prefix)
        if ({str(path.relative_to(ROOT)): digest(path) for path in inputs} != before_hashes
                or digest(archive) != archive_hash
                or (overlay and digest(overlay) != overlay_hash)):
            raise RuntimeError('RGB source or archive changed during native run')
        if original != migrated:
            raise RuntimeError('frozen/current RGB fixture transcripts differ')
        receipt['transcript_sha256'] = hashlib.sha256(original).hexdigest()
        receipt['lines'] = len(original.splitlines())
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
