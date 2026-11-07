#!/usr/bin/env python3
"""Build and observe pinned frozen/current Nebula allocation-failure fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/nebula-renderer'
STAGE8 = ROOT / 'build/rust-migration-stage8'
CHECKPOINT = ROOT / 'build/rust-migration-stage10/fire-arcs/checkpoint-v2'
UNIT = Path('modules/A2FONebulaRenderer/module.cpp')
AUX = Path('modules/A2FONebulaRenderer/art_texture_suffix_config.cpp')
BRIDGE = ROOT / 'modules/A2FONebulaRenderer/thiscall_bridge.S'
FROZEN = STAGE8 / 'before' / UNIT
CURRENT = ROOT / UNIT
ORIGINAL_LIB = CHECKPOINT / 'liba2fo_core-win.a'
CURRENT_LIB = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
FIXTURE = ROOT / 'tests/rust_stage10_nebula_oom.cpp'
MOCK = ROOT / 'tests/rust_stage8_c_nebula_core_mock.cpp'
ORIGINAL_SHA = '01258fbad72411f37bebabffc2efb3e70aaeb8d7b28cf2a6f32a0558f2357c36'
EXPECTED_PREFIXES = ('trim-substr:', 'query-copy:', 'query-substr:',
                     'bounded-reserve:', 'bounded-growth:', 'node-growth:')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(args, output, label, *, cwd=ROOT, env=None):
    with (output / f'{label}.stdout.log').open('wb') as stdout, \
         (output / f'{label}.stderr.log').open('wb') as stderr:
        result = subprocess.run(args, cwd=cwd, env=env, stdout=stdout,
                                stderr=stderr, timeout=300, check=False)
    if result.returncode:
        raise RuntimeError(f'{label} exited {result.returncode}; inspect {output}')


def dependencies(source, output, label):
    args = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
            f'-DA2FO_FIXTURE_SOURCE="{source}"', str(FIXTURE)]
    text = subprocess.run(args, cwd=ROOT, capture_output=True, text=True,
                          timeout=60, check=True).stdout
    (output / f'{label}-dependencies.txt').write_text(text)
    return sorted({Path(item).resolve() for item in
                   text.replace('\\\n', ' ').split(':', 1)[1].split()
                   if Path(item).resolve().is_relative_to(ROOT)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output.exists() or output.parent != SLICE or not output.name.startswith('oom-'):
        parser.error('output must be a fresh oom-* child of the Nebula slice')
    if not args.build_only and args.wine_prefix is None:
        parser.error('--wine-prefix required for execution')
    frozen_pins = json.loads((STAGE8 / 'before-hashes.json').read_text())
    checkpoint_pins = json.loads((CHECKPOINT / 'source-hashes.json').read_text())
    if sha(FROZEN) != frozen_pins[str(UNIT)] or sha(ORIGINAL_LIB) != ORIGINAL_SHA:
        raise RuntimeError('frozen source or archive differs from authenticated pin')
    if sha(STAGE8 / 'before' / AUX) != frozen_pins[str(AUX)]:
        raise RuntimeError('frozen auxiliary differs from authenticated pin')
    if sha(ROOT / AUX) != frozen_pins[str(AUX)]:
        raise RuntimeError('current auxiliary differs from frozen pin')
    output.mkdir(parents=True)
    paths = {
        'checker': Path(__file__), 'fixture': FIXTURE,
        'full_fixture': ROOT / 'tests/rust_stage10_nebula_native.cpp',
        'stage8_fixture': ROOT / 'tests/rust_stage8_c_nebula_native.cpp',
        'mock': MOCK, 'bridge': BRIDGE, 'frozen_source': FROZEN,
        'current_source': CURRENT, 'frozen_aux': STAGE8 / 'before' / AUX,
        'current_aux': ROOT / AUX, 'original_archive': ORIGINAL_LIB,
        'candidate_archive': CURRENT_LIB,
        'frozen_manifest': STAGE8 / 'before-hashes.json',
        'checkpoint_manifest': CHECKPOINT / 'source-hashes.json',
        'rust_lib_source': ROOT / 'rust/a2fo_core/src/lib.rs',
    }
    for path in ROOT.glob('rust/a2fo_core/src/stage10_nebula*.rs'):
        paths[path.relative_to(ROOT).as_posix()] = path
    for path in ROOT.glob('rust/stage10_nebula*.h'):
        paths[path.relative_to(ROOT).as_posix()] = path
    deps = {}
    for label, source in (('before', FROZEN), ('current', CURRENT)):
        deps[label] = dependencies(source, output, label)
        for path in deps[label]:
            paths.setdefault(f'dependency:{path.relative_to(ROOT)}', path)
    hashes = {name: sha(path) for name, path in paths.items()}
    if hashes['frozen_manifest'] != '309715d185546d660e64949b8490f45501bf9c2bd3d922b5f8c4c469325239db':
        raise RuntimeError('frozen manifest drift')
    if hashes['checkpoint_manifest'] != '05a530c884473ac4cdc58196ed678d497806fbf1e38e3161c52355cef5c51837':
        raise RuntimeError('checkpoint manifest drift')
    source_used = output / 'source-used'
    for name, path in paths.items():
        if name.endswith('archive'):
            continue
        target = source_used / name / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        if sha(target) != hashes[name]:
            raise RuntimeError(f'snapshot drift: {name}')
    for label, sources in deps.items():
        for path in sources:
            relative = path.relative_to(ROOT).as_posix()
            if path in (FIXTURE, paths['full_fixture'], paths['stage8_fixture'],
                        FROZEN, CURRENT):
                continue
            key = relative.removeprefix('build/rust-migration-stage8/before/')
            pins = frozen_pins if label == 'before' else checkpoint_pins
            if key.startswith('rust/stage10_nebula'):
                expected = hashes[f'dependency:{relative}']
            else:
                expected = pins.get(key)
            if expected is None or sha(path) != expected:
                raise RuntimeError(f'{label} unpinned dependency: {relative}')
    binaries = {}
    command(['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
             '-Werror', '-shared', '-static', '-static-libgcc', '-static-libstdc++',
             str(MOCK), '-o', str(output / 'A2FOExtensions.dll')], output,
            'build-mock')
    binaries['A2FOExtensions.dll'] = sha(output / 'A2FOExtensions.dll')
    for label, source, aux, lib in (
        ('before', FROZEN, STAGE8 / 'before' / AUX, ORIGINAL_LIB),
        ('current', CURRENT, ROOT / AUX, CURRENT_LIB),
    ):
        command(['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
                 '-Wextra', '-Werror', '-Wno-error=mismatched-new-delete',
                 '-DNDEBUG', '-static', '-static-libgcc', '-static-libstdc++',
                 f'-DA2FO_FIXTURE_SOURCE="{source}"', str(FIXTURE), str(aux),
                 str(BRIDGE), str(lib), '-o', str(output / f'{label}.exe')],
                output, f'build-{label}')
        binaries[f'{label}.exe'] = sha(output / f'{label}.exe')
    def verify():
        for name, path in paths.items():
            if sha(path) != hashes[name]:
                raise RuntimeError(f'input drift: {name}')
        for name, digest in binaries.items():
            if sha(output / name) != digest:
                raise RuntimeError(f'binary drift: {name}')
    verify()
    receipt = {'inputs': hashes, 'dependencies': {
        k: [p.relative_to(ROOT).as_posix() for p in v] for k, v in deps.items()},
        'binaries': binaries, 'original_archive_sha256': ORIGINAL_SHA}
    (output / 'inputs.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if args.build_only:
        print(f'PASS built frozen/current Nebula OOM fixtures: {output}')
        return
    prefix = args.wine_prefix.resolve()
    if not (prefix / 'system.reg').is_file() or not (prefix / 'drive_c').is_dir():
        raise RuntimeError('initialized private Wine prefix required')
    env = dict(os.environ)
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
               LIBGL_ALWAYS_SOFTWARE='1',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                  'winewayland.drv=d;winex11.drv=d')
    observed = {}
    for label in ('before', 'current'):
        verify()
        case_dir = output / f'case-{label}'
        case_dir.mkdir()
        shutil.copy2(output / 'A2FOExtensions.dll', case_dir / 'A2FOExtensions.dll')
        command(['wine', str(output / f'{label}.exe')], output, f'run-{label}',
                cwd=case_dir, env=env)
        lines = (output / f'run-{label}.stdout.log').read_text().splitlines()
        if len(lines) != len(EXPECTED_PREFIXES) or any(
            not line.startswith(prefix) for line, prefix in zip(lines, EXPECTED_PREFIXES)
        ):
            raise RuntimeError(f'{label} incomplete OOM observations: {lines}')
        observed[label] = lines
    verify()
    receipt['observed'] = observed
    receipt['logs'] = {f'{label}.{stream}': sha(output / f'run-{label}.{stream}.log')
                       for label in ('before', 'current') for stream in ('stdout', 'stderr')}
    receipt['equal'] = observed['before'] == observed['current']
    (output / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if not receipt['equal']:
        raise RuntimeError(f'Nebula OOM mismatch: {observed}')
    print(f'PASS Nebula OOM: {len(EXPECTED_PREFIXES)} cases, exact outcomes equal')


if __name__ == '__main__':
    main()
