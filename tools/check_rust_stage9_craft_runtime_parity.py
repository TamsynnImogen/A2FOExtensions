#!/usr/bin/env python3
"""Frozen/current full-source headless CraftIdentity native fixture."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/craft-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOCraftIdentity/module.cpp'
CURRENT = ROOT / 'modules/A2FOCraftIdentity/module.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_craft_runtime.rs'
HEADER = ROOT / 'rust/stage9_craft_runtime_ffi.h'
SPRITE_RUST = ROOT / 'rust/a2fo_core/src/stage9_craft_sprite.rs'
SPRITE_HEADER = ROOT / 'rust/stage9_craft_sprite_ffi.h'
TEST = ROOT / 'tests/rust_stage9_craft_runtime_native.cpp'
BASE = ROOT / 'tests/craft_identity_ammunition_hover_test.cpp'
ARCHIVE = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
FROZEN_SHA = '560333f01b25d60b164afdbc602cd9c8d241f47783d6b1c3015c9cbb807d001f'
HELPERS = tuple(path for path in sorted((ROOT / 'modules/A2FOCraftIdentity').glob('*.cpp'))
                if path != CURRENT)
BRIDGE = ROOT / 'modules/A2FOCraftIdentity/thiscall_bridge.S'
PINS = (FROZEN, CURRENT, RUST, HEADER, SPRITE_RUST, SPRITE_HEADER,
        TEST, BASE, BRIDGE, *HELPERS,
        *(ROOT / 'modules/A2FOCraftIdentity' / name for name in
          ('identity_selection.hpp', 'object_editor.hpp', 'selected_panel_anchor.hpp')),
        ROOT / 'sdk/include/a2fo_module_api.h')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint(archive):
    return {str(path.relative_to(ROOT)): digest(path) for path in (*PINS, archive)}


def build(version, source, archive, rust_objects):
    exe = WORK / f'craft-{version}.exe'
    base_text = BASE.read_text()
    signature = 'int main() {'
    if base_text.count(signature) != 1:
        raise RuntimeError('base hover fixture main changed')
    generated = WORK / f'craft-{version}-base.cpp'
    generated.write_text(base_text.replace(
        signature, 'void stage9_existing_craft_hover_main() {'))
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
               '-Wpedantic', '-Werror', '-UNDEBUG', '-DWIN32_LEAN_AND_MEAN',
               '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
               '-Wl,--image-base,0x10000000', '-static', '-static-libgcc',
               '-static-libstdc++',
               f'-DA2FO_AMMUNITION_MODULE_SOURCE="{source}"',
               f'-DCRAFT_BASE_SOURCE="{generated}"', str(TEST),
               *(str(path) for path in HELPERS), str(BRIDGE),
               *(str(obj) for obj in rust_objects),
               str(archive), '-ladvapi32', '-lgdi32', '-luser32', '-o', str(exe)]
    subprocess.run(command, cwd=ROOT, check=True, timeout=300)
    return exe


def run_wine(exe, prefix):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = ('winemenubuilder.exe=d;winedbg.exe=d;'
                              'winewayland.drv=d;winex11.drv=d')
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        proc = subprocess.run(['wine', str(exe)], cwd=ROOT, env=env,
                              stdout=output, stderr=errors, timeout=180)
        output.seek(0)
        errors.seek(0)
        trace = output.read().replace(b'\r\n', b'\n')
        err = errors.read()
    required = (b'hover:inactive-reset\n',
                b'render:aliased-name-reverse-restore\n',
                b'hover:resampled-visibility-and-text-pointers\n',
                b'sprite:retry-gate-and-missing-database\n',
                b'sprite:icon-live-cache-slot\n',
                b'sprite:icon-crop-bounds\n',
                b'sprite:icon-stale-quiet-then-retry\n',
                b'sprite:bar-snapshot-and-width-before-colour\n',
                b'sprite:bar-requested-fallback-and-foreground\n',
                b'anchor:single-captain:', b'anchor:builder-captain:',
                b'anchor:single-name-rebased:', b'anchor:builder-class-rebased:',
                b'anchor:single-name-unconfigured:',
                b'anchor:hidden-captain-raw:',
                b'anchor:mutation-read-order:', b'anchor:null-display\n',
                b'join:empty-left:', b'join:empty-right:',
                b'join:existing-separator:', b'join:insert-separator:')
    if proc.returncode or any(marker not in trace for marker in required):
        raise RuntimeError(f'{exe.name}: exit={proc.returncode}; stdout={trace[-2000:]!r}; '
                           f'stderr={err[-2000:]!r}')
    return trace


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', ARCHIVE)))
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    '/tmp/a2fo-craft-wine-prefix')))
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    if digest(FROZEN) != FROZEN_SHA:
        parser.error('frozen CraftIdentity source drift')
    if not archive.is_file():
        parser.error(f'Rust archive missing: {archive}')
    initial = fingerprint(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    symbols = subprocess.check_output(['i686-w64-mingw32-nm', '-g', '--defined-only', str(archive)], cwd=ROOT, text=True)
    exported = {line.split()[-1] for line in symbols.splitlines()
                if line.split()}
    have_runtime = '_a2fo_rs_stage9_craft_copy_identity_rows' in exported
    have_sprite = '_a2fo_rs_stage9_craft_sprite_flow' in exported
    have_anchor = ({'_a2fo_rs_stage9_craft_anchor',
                    '_a2fo_rs_stage9_craft_anchor_fallback'} <= exported)
    if len({have_runtime, have_sprite, have_anchor}) != 1:
        parser.error('archive has a partial Craft runtime; rebuild canonical archive')
    objects = []
    if not have_runtime:
        for source, name in ((RUST, 'stage9_craft_runtime.o'),
                             (SPRITE_RUST, 'stage9_craft_sprite.o')):
            obj = WORK / name
            subprocess.run(['rustc', '--target=i686-pc-windows-gnu',
                            '--edition=2024', '-D', 'warnings', '--emit=obj',
                            '--crate-type=lib', str(source), '-o', str(obj)],
                           cwd=ROOT, check=True, timeout=120)
            objects.append(obj)
    built = {version: build(version, source, archive, objects)
             for version, source in (('frozen', FROZEN), ('current', CURRENT))}
    if fingerprint(archive) != initial:
        parser.error('input changed during fixture build')
    receipt = {'pins': initial,
               'rust_objects': {obj.name: digest(obj) for obj in objects},
               'fixtures': {key: digest(value) for key, value in built.items()},
               'build_only': args.build_only}
    if not args.build_only:
        traces = {}
        for key, exe in built.items():
            traces[key] = run_wine(exe, args.wine_prefix)
            (WORK / f'{key}.trace').write_bytes(traces[key])
        if traces['frozen'] != traces['current']:
            old, new = traces['frozen'].splitlines(), traces['current'].splitlines()
            for index in range(max(len(old), len(new))):
                if old[index:index+1] != new[index:index+1]:
                    raise RuntimeError(f'frozen/current native trace mismatch line {index+1}: '
                                       f'{old[index:index+1]!r} != {new[index:index+1]!r}')
            raise RuntimeError('frozen/current native trace mismatch')
        if fingerprint(archive) != initial:
            parser.error('input changed during native run')
        (WORK / 'trace.txt').write_bytes(traces['current'])
        receipt['trace_sha256'] = hashlib.sha256(traces['current']).hexdigest()
        receipt['trace_lines'] = len(traces['current'].splitlines())
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
