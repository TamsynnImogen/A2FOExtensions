#!/usr/bin/env python3
"""Build and compare frozen/current RendererOptions actual-source fixtures."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/renderer-options-work'
SOURCE = ROOT / 'core/renderer_options.cpp'
FROZEN = ROOT / 'build/rust-migration-stage8/before/core/renderer_options.cpp'
HASHES = ROOT / 'build/rust-migration-stage8/before-hashes.json'
FIXTURE = ROOT / 'tests/rust_stage9_renderer_options_native.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_renderer_options.rs'
HEADER = ROOT / 'rust/stage9_renderer_options_ffi.h'
SCRIPT = Path(__file__).resolve()
DEFAULT_BASE = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
REQUIRED_MARKERS = (
    b'renderer stage9: typed FormShow gateway exception preserved',
    b'renderer stage9: empty, 16 KiB boundary and failed opens passed',
    b'renderer stage9: combo log exception caught before selection',
    b'renderer stage9: reentrant combo admission and priority passed',
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture_for(source, label):
    marker = '#include "../core/renderer_options.cpp"'
    original = FIXTURE.read_text()
    if original.count(marker) != 1:
        raise RuntimeError('renderer fixture source marker changed')
    generated = WORK / f'{label}-renderer-options.cpp'
    generated.write_text(original.replace(marker, f'#include "{source}"'))
    return generated


def compile_fixture(source, label, base, overlay=None):
    target = WORK / f'{label}.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
               '-Wextra', '-Wpedantic', '-Werror', '-DNDEBUG', '-DNOMINMAX',
               '-DWIN32_LEAN_AND_MEAN', '-static', '-static-libgcc',
               '-static-libstdc++', '-I', str(ROOT / 'core'),
               str(fixture_for(source, label))]
    if overlay:
        command.append(str(overlay))
    command += [str(base), '-lcomctl32', '-lshlwapi', '-lole32', '-lws2_32',
                '-lntdll', '-luserenv', '-lbcrypt', '-ladvapi32', '-o', str(target)]
    with (WORK / f'{label}-compile.stdout').open('wb') as stdout, \
            (WORK / f'{label}-compile.stderr').open('wb') as stderr:
        result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr,
                                timeout=300)
    if result.returncode:
        raise RuntimeError(f'{label} compile exit {result.returncode}: '
                           f'{(WORK / f"{label}-compile.stderr").read_bytes()[-4000:]!r}')
    return target


def run_native(executable, fleetops, prefix, layout):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    argv = ['wine', str(executable), str(fleetops), str(fleetops)]
    if layout == 'clean':
        argv.append('--clean-layout')
    stdout_path = WORK / f'{executable.stem}-{layout}.stdout'
    stderr_path = WORK / f'{executable.stem}-{layout}.stderr'
    with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
        result = subprocess.run(argv, cwd=WORK, env=env, stdout=stdout,
                                stderr=stderr, timeout=180)
    if result.returncode:
        raise RuntimeError(f'{executable.name} {layout} exit {result.returncode}: '
                           f'{stderr_path.read_bytes()[-4000:]!r}')
    return stdout_path.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_BASE)))
    parser.add_argument('--renderer-rlib', type=Path,
                        help='isolated stage-nine renderer rlib for the current fixture')
    parser.add_argument('--fleetops', type=Path, default=ROOT / 'FleetOpsHook.dll')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    WORK / 'wine-prefix')))
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    base = args.base_lib.resolve()
    overlay = args.renderer_rlib.resolve() if args.renderer_rlib else None
    expected = json.loads(HASHES.read_text())['core/renderer_options.cpp']
    if digest(FROZEN) != expected:
        parser.error('frozen RendererOptions source drift')
    if not base.is_file() or (overlay and not overlay.is_file()):
        parser.error('Rust archive missing')
    defined = subprocess.check_output(
        ['i686-w64-mingw32-nm', '--defined-only', str(base)], text=True)
    integrated = any(line.split()[-1].lstrip('_') == 'a2fo_rs_stage9_renderer_join'
                     for line in defined.splitlines() if line.split())
    if overlay:
        if integrated:
            parser.error('base archive already has renderer export; omit overlay')
        if not args.build_only:
            parser.error('isolated renderer overlay is build-only')
        if overlay.stat().st_mtime_ns < max(RUST.stat().st_mtime_ns,
                                           HEADER.stat().st_mtime_ns):
            parser.error('isolated renderer rlib predates source/header')
    else:
        if not integrated:
            parser.error('canonical archive lacks stage-nine renderer export')
        if base.stat().st_mtime_ns < max(RUST.stat().st_mtime_ns,
                                        HEADER.stat().st_mtime_ns):
            parser.error('canonical archive predates renderer source/header')
    if not args.build_only and not args.fleetops.resolve().is_file():
        parser.error('FleetOpsHook.dll required for native fixture')
    inputs = (SOURCE, FROZEN, HASHES, FIXTURE, RUST, HEADER, SCRIPT, base) + \
        ((overlay,) if overlay else ())
    if not args.build_only:
        inputs += (args.fleetops.resolve(),)
    initial = {str(path): digest(path) for path in inputs}
    WORK.mkdir(parents=True, exist_ok=True)
    before = compile_fixture(FROZEN, 'before', base)
    current = compile_fixture(SOURCE, 'current', base, overlay)
    if {str(path): digest(path) for path in inputs} != initial:
        raise RuntimeError('renderer inputs changed during build')
    receipt = {'mode': 'build-only' if args.build_only else 'native-parity',
               'inputs_sha256': initial,
               'before_sha256': digest(before),
               'current_sha256': digest(current)}
    if not args.build_only:
        prefix = args.wine_prefix.resolve()
        fleetops = args.fleetops.resolve()
        if not (prefix / 'system.reg').is_file() or not fleetops.is_file():
            parser.error('initialized private prefix and FleetOpsHook.dll required')
        receipt['transcripts'] = {}
        for layout in ('clean', 'roots'):
            original = run_native(before, fleetops, prefix, layout)
            migrated = run_native(current, fleetops, prefix, layout)
            if {str(path): digest(path) for path in inputs} != initial:
                raise RuntimeError('renderer inputs changed during native run')
            for label, transcript in (('before', original), ('current', migrated)):
                for marker in REQUIRED_MARKERS:
                    if marker not in transcript:
                        raise RuntimeError(f'{label} {layout} missing marker {marker!r}')
            if original != migrated:
                raise RuntimeError(f'{layout} renderer fixture transcripts differ; '
                                   f'see {WORK / f"before-{layout}.stdout"} and '
                                   f'{WORK / f"current-{layout}.stdout"}')
            receipt['transcripts'][layout] = hashlib.sha256(original).hexdigest()
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
