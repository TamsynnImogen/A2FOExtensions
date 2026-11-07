#!/usr/bin/env python3
"""Pin, build, and run two FireArcs native allocation-failure fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/fire-arcs'
PREVIOUS = SLICE / 'fixtures-native-v4/inputs.json'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOFireArcs/module.cpp'
CURRENT = ROOT / 'modules/A2FOFireArcs/module.cpp'
ORIGINAL_LIB = ROOT / 'build/rust-migration-stage9/checkpoint-archive10/liba2fo_core-win.a'
CANDIDATE_LIB = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
FIXTURE = ROOT / 'tests/rust_stage10_fire_arcs_oom.cpp'
AUX = [ROOT / 'modules/A2FOFireArcs/fire_arc.cpp',
       ROOT / 'modules/A2FOFireArcs/runtime_config.cpp']
EXPECTED_LINES = [
    'trim:bad_alloc:unchanged',
    'query-invalid:false:absent:empty:no-allocation',
    'query-assign:false:present:empty',
    'query-trim:false:present:empty',
    'root:bad_alloc',
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, directory, label, env=None):
    with (directory / f'{label}.stdout.log').open('wb') as stdout, \
         (directory / f'{label}.stderr.log').open('wb') as stderr:
        completed = subprocess.run(args, cwd=ROOT, env=env, stdout=stdout,
                                   stderr=stderr, timeout=300, check=False)
    if completed.returncode:
        raise RuntimeError(f'{label} exit {completed.returncode}; see {directory}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output.exists() or output.parent != SLICE or not output.name.startswith('oom-'):
        parser.error('output must be a new oom-* child of FireArcs slice')
    if not args.build_only and args.wine_prefix is None:
        parser.error('--wine-prefix required for execution')
    previous = json.loads(PREVIOUS.read_text())['sources']
    static = {
        'legacy_fixture': ROOT / 'tests/rust_stage10_fire_arcs_native.cpp',
        'frozen_source': FROZEN, 'current_source': CURRENT,
        'original_archive': ORIGINAL_LIB, 'candidate_archive': CANDIDATE_LIB,
        'fire_arc_source': AUX[0], 'runtime_config_source': AUX[1],
        'rust_lib_source': ROOT / 'rust/a2fo_core/src/lib.rs',
        'text_rust_source': ROOT / 'rust/a2fo_core/src/stage10_fire_arcs_text.rs',
        'text_ffi_header': ROOT / 'rust/stage10_fire_arcs_text_ffi.h',
    }
    for name, old_name in [('legacy_fixture', 'fixture'),
                           ('text_rust_source', 'rust/a2fo_core/src/stage10_fire_arcs_text.rs'),
                           ('text_ffi_header', 'rust/stage10_fire_arcs_text_ffi.h')]:
        if digest(static[name]) != previous[old_name]:
            raise RuntimeError(f'{name} drifted from native-v4 receipt')
    for name in static:
        if name in previous and digest(static[name]) != previous[name]:
            raise RuntimeError(f'{name} drifted from native-v4 receipt')
    frozen_pins = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())
    current_pins = json.loads((ROOT / 'build/rust-migration-stage9/checkpoint-archive10/source-hashes.json').read_text())
    output.mkdir(parents=True)
    sources = {'oom_fixture': FIXTURE, 'oom_checker': Path(__file__), **static}
    fingerprints = {name: digest(path) for name, path in sources.items()}
    snapshots = output / 'source-used'
    snapshots.mkdir()
    for name in ('oom_fixture', 'oom_checker', 'legacy_fixture', 'frozen_source',
                 'current_source', 'fire_arc_source', 'runtime_config_source',
                 'rust_lib_source', 'text_rust_source', 'text_ffi_header'):
        target = snapshots / name / sources[name].name
        target.parent.mkdir(parents=True)
        shutil.copy2(sources[name], target)
        if digest(target) != fingerprints[name]:
            raise RuntimeError(f'{name} snapshot differs')
    dependency_hashes = {}
    binaries = {}
    for version, source, archive, pins in (
        ('before', FROZEN, ORIGINAL_LIB, frozen_pins),
        ('current', CURRENT, CANDIDATE_LIB, current_pins),
    ):
        defines = [f'-DMODULE_SOURCE="{source}"']
        if version == 'current':
            defines.append('-DFIRE_ARCS_CURRENT')
        dep_command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
                       *defines, str(FIXTURE)]
        dep_text = subprocess.run(dep_command, cwd=ROOT, capture_output=True,
                                  text=True, timeout=60, check=True).stdout
        (output / f'{version}-dependencies.txt').write_text(dep_text)
        names = dep_text.replace('\\\n', ' ').split(':', 1)[1].split()
        dependencies = sorted({
            Path(name).resolve() for name in names
            if Path(name).resolve().is_relative_to(ROOT)
        })
        for dep in dependencies:
            relative = dep.relative_to(ROOT).as_posix()
            if dep in (FIXTURE, sources['legacy_fixture'], source):
                continue
            key = relative.removeprefix('build/rust-migration-stage8/before/')
            expected = pins.get(key)
            if key in previous and key.startswith('rust/stage10_fire_arcs'):
                expected = previous[key]
            if expected is None or digest(dep) != expected:
                raise RuntimeError(f'{version} unpinned dependency {relative}')
            dependency_hashes[f'{version}/{relative}'] = expected
            target = snapshots / version / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dep, target)
            if digest(target) != expected:
                raise RuntimeError(f'{version} dependency snapshot differs: {relative}')
        exe = output / f'{version}.exe'
        command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
                   '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                   '-Wno-error=mismatched-new-delete',
                   '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                   '-static', *defines, str(FIXTURE), *(str(x) for x in AUX),
                   str(archive), '-lws2_32', '-lbcrypt', '-lole32',
                   '-ladvapi32', '-o', str(exe)]
        run(command, output, f'build-{version}')
        binaries[version] = digest(exe)
    def verify():
        for name, path in sources.items():
            if digest(path) != fingerprints[name]:
                raise RuntimeError(f'input drifted: {name}')
        for version, sha in binaries.items():
            if digest(output / f'{version}.exe') != sha:
                raise RuntimeError(f'binary drifted: {version}')
        for key, sha in dependency_hashes.items():
            if digest(ROOT / key.split('/', 1)[1]) != sha:
                raise RuntimeError(f'dependency drifted: {key}')
    verify()
    receipt = {'sources': fingerprints, 'dependency_hashes': dependency_hashes,
               'binaries': binaries, 'cases': EXPECTED_LINES}
    (output / 'inputs.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if args.build_only:
        print(f'PASS built 2 OOM fixtures at {output}')
        return
    prefix = args.wine_prefix.resolve()
    if not (prefix / 'system.reg').is_file() or not (prefix / 'drive_c').is_dir():
        raise RuntimeError('initialized Wine prefix required')
    env = dict(os.environ)
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
               LIBGL_ALWAYS_SOFTWARE='1',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                  'winewayland.drv=d;winex11.drv=d')
    results = {}
    for version in ('before', 'current'):
        run(['wine', str(output / f'{version}.exe')], output, f'run-{version}', env)
        lines = (output / f'run-{version}.stdout.log').read_text().splitlines()
        if lines != EXPECTED_LINES:
            raise RuntimeError(f'{version} incorrect observed outcomes: {lines!r}')
        results[version] = lines
    if results['before'] != results['current']:
        raise RuntimeError('native OOM outcomes differ')
    verify()
    receipt['results'] = results
    receipt['logs'] = {
        f'{version}.{stream}': digest(output / f'run-{version}.{stream}.log')
        for version in ('before', 'current') for stream in ('stdout', 'stderr')
    }
    (output / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS FireArcs OOM: 5 assertions each, 2 binaries, exact outcomes equal')


if __name__ == '__main__':
    main()
