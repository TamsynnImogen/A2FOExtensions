#!/usr/bin/env python3
"""Frozen/current full-source native differential for texture ODF visuals."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/texture-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOTextureVariants/module.cpp'
CURRENT = ROOT / 'modules/A2FOTextureVariants/module.cpp'
HELPER = ROOT / 'modules/A2FOTextureVariants/texture_variants.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_texture_visuals.rs'
STAGE8 = ROOT / 'rust/a2fo_core/src/stage8_texture_runtime.rs'
HEADER = ROOT / 'rust/stage9_texture_visuals_ffi.h'
TEST = ROOT / 'tests/rust_stage9_texture_visuals_native.cpp'
FROZEN_SHA256 = '315654de9ec77558b1d923c03a092ea07666fed379e4ae560f5728fa7dd7ef0d'
DEFAULT_ARCHIVE = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
MODES = ('odf-mesh', 'odf-scorch', 'odf-invalid', 'odf-empty',
         'odf-mutate', 'odf-exception', 'update-init', 'update-destroy',
         'update-repair', 'update-operational', 'update-no-policy',
         'update-no-systems', 'update-reset', 'render-hidden',
         'render-half', 'render-mutate', 'render-none', 'walk-faction',
         'walk-mesh', 'walk-subtree', 'walk-resolve', 'walk-admission',
         'walk-partial', 'shutdown', 'init-invalid', 'race-valid',
         'race-invalid', 'race-absent', 'race-exception', 'hit', 'miss',
         'cache', 'mutate-miss', 'borg', 'base', 'node-show', 'node-hide',
         'node-mutate', 'node-admission', 'disabled', 'null')
MODES += ('select-empty', 'select-choice', 'select-scan-mutate',
          'selected-valid', 'selected-stale', 'select-admission',
          'race-field-exception', 'cleanup-multi', 'cleanup-reuse',
          'render-grow', 'render-plan-partial', 'walk-resolve-mutate')
PINS = (FROZEN, CURRENT, HELPER, RUST, STAGE8, HEADER, TEST)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pins(archive):
    return {str(path): digest(path) for path in (*PINS, archive)}


def build(source, archive, rust_object, exe, layered_archive=False):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
        '-Wpedantic', '-Werror', '-Wno-array-bounds',
        '-Wno-mismatched-new-delete', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source}"', str(TEST), str(HELPER),
    ]
    if rust_object is not None:
        command.append(str(rust_object))
    if layered_archive:
        # Build-only compatibility with a stale integrated archive. The
        # isolated object precedes it; the canonical archive must be rebuilt
        # before runtime parity is credited.
        command.append('-Wl,--allow-multiple-definition')
    command += [str(archive), '-ladvapi32', '-o', str(exe)]
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
    if not output or (b'policy:' not in output and b'shutdown-flags:' not in output) or \
            (b'visual:' not in output and b'shutdown-maps:' not in output):
        raise RuntimeError(f'{exe.name}/{mode} lacks expected state trace')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_ARCHIVE)))
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_PREFIX',
                                                    os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    '/tmp/a2fo-texture-wine-prefix'))))
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    if digest(FROZEN) != FROZEN_SHA256:
        parser.error('frozen TextureVariants source drift')
    if not archive.is_file():
        parser.error(f'Rust archive missing: {archive}')
    if archive.stat().st_mtime_ns < STAGE8.stat().st_mtime_ns:
        parser.error('Rust archive predates stage8 texture runtime source')
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
    rust_object = None
    if any(symbol not in symbols for symbol in (
        '_a2fo_rs_stage9_texture_register',
        '_a2fo_rs_stage9_texture_select_mesh',
        '_a2fo_rs_stage9_texture_selected_mesh',
    )):
        rust_object = WORK / 'stage9_texture_visuals.o'
        subprocess.run(['rustc', '--target=i686-pc-windows-gnu',
                        '--edition=2024', '--emit=obj', '--crate-type=lib',
                        str(RUST), '-o', str(rust_object)], cwd=ROOT, check=True,
                       timeout=120)
    layered_archive = rust_object is not None and \
        '_a2fo_rs_stage9_texture_register' in symbols
    if layered_archive and not args.build_only:
        parser.error('integrated Rust archive predates stage9 texture source')
    exes = {}
    for version, source in (('frozen', FROZEN), ('current', CURRENT)):
        exe = WORK / f'texture-{version}.exe'
        build(source, archive, rust_object, exe, layered_archive)
        if pins(archive) != initial:
            parser.error('source or archive drift during fixture build')
        exes[version] = exe
    receipt = {
        'pins': initial,
        'fixtures': {version: digest(exe) for version, exe in exes.items()},
        'rust_object': digest(rust_object) if rust_object else None,
        'build_only': args.build_only,
        'layered_archive': layered_archive,
        'headless': {
            'display_unset': True,
            'wayland_display_unset': True,
            'wine_prefix': str(args.wine_prefix),
        },
        'modes': [],
    }
    if not args.build_only:
        for mode in MODES:
            frozen = run_wine(exes['frozen'], mode, args.wine_prefix)
            current = run_wine(exes['current'], mode, args.wine_prefix)
            (WORK / f'{mode}-frozen.txt').write_bytes(frozen)
            (WORK / f'{mode}-current.txt').write_bytes(current)
            if frozen != current:
                raise RuntimeError(f'{mode}: frozen/current native trace mismatch')
            if mode == 'odf-mutate' and b'mutate:engineMesh1' not in current:
                raise RuntimeError('ODF mutation did not occur')
            if mode == 'odf-exception' and b'injection:used' not in current:
                raise RuntimeError('ODF exception injection was not exercised')
            if mode == 'odf-exception':
                destructors = re.search(rb'^exception-destructors:(\d+)$',
                                        current, re.MULTILINE)
                if not destructors or int(destructors[1]) == 0 or b'policy:0' not in current:
                    raise RuntimeError('ODF exception recovery trace invalid')
            if mode == 'walk-partial' and (
                b'walk-exception:typed' not in current or
                b'walk-injection:used' not in current or
                b'faction-cache:0' not in current
            ):
                raise RuntimeError('partial traversal failure was not exercised')
            if mode == 'walk-subtree' and b'subtree-node:walk-outside' in current:
                raise RuntimeError('subtree traversal claimed an unrelated sibling')
            if mode == 'shutdown' and (b'shutdown-flags:0:0:0:0' not in current or
                                       b'shutdown-maps:0:0:0:0' not in current):
                raise RuntimeError('shutdown did not clear flags and maps')
            if mode == 'render-grow' and b'render-count:3' not in current:
                raise RuntimeError('render subtree growth was not observed')
            if mode == 'render-plan-partial':
                before = re.search(rb'^partial-before-flags:(\d+)$',
                                   current, re.MULTILINE)
                count = re.search(rb'^render-count:(\d+)$',
                                  current, re.MULTILINE)
                if (b'render-injection:used' not in current or
                    not before or int(before[1]) == 0 or
                    b'partial-before-scale:0.000000' not in current or
                    not count or int(count[1]) == 0 or
                    b'partial-after-flags:0' not in current or
                    b'partial-after-scale:2.000000' not in current):
                    raise RuntimeError('post-write native query failure or reverse restoration unproven')
            if mode == 'walk-resolve-mutate' and (
                b'mutate:resolve-second=fed' not in current or
                b'resolved-second:walk-root' not in current
            ):
                raise RuntimeError('resolve mutation was not observed')
            if mode == 'select-scan-mutate' and b'choice-entries:2' not in current:
                raise RuntimeError('selection scan mutation was not observed')
            if mode == 'race-field-exception' and (
                b'field-exception:typed' not in current or
                b'field-injection:used' not in current
            ):
                raise RuntimeError('typed race-field exception was not exercised')
            if mode == 'cleanup-multi' and b'cleanup-count:1:1' not in current:
                raise RuntimeError('multiple cleanup sidecars were not removed')
            if mode == 'cleanup-reuse' and b'reuse-count:2:2' not in current:
                raise RuntimeError('reused sidecar identity was not retained')
            if pins(archive) != initial:
                parser.error('source or archive drift during fixture run')
            receipt['modes'].append({
                'name': mode, 'trace_sha256': hashlib.sha256(current).hexdigest(),
            })
    (WORK / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
