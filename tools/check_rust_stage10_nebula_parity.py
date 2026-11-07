#!/usr/bin/env python3
"""Build and compare frozen/current full-source NebulaRenderer native fixtures."""
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
UNIT = Path('modules/A2FONebulaRenderer/module.cpp')
AUX = Path('modules/A2FONebulaRenderer/art_texture_suffix_config.cpp')
BRIDGE = Path('modules/A2FONebulaRenderer/thiscall_bridge.S')
FIXTURE = ROOT / 'tests/rust_stage10_nebula_native.cpp'
OLD_FIXTURE = ROOT / 'tests/rust_stage8_c_nebula_native.cpp'
MOCK = ROOT / 'tests/rust_stage8_c_nebula_core_mock.cpp'
ORIGINAL_ARCHIVE = ROOT / 'build/rust-migration-stage10/fire-arcs/checkpoint-v2/liba2fo_core-win.a'
ORIGINAL_SHA = '01258fbad72411f37bebabffc2efb3e70aaeb8d7b28cf2a6f32a0558f2357c36'
MARKERS = (b'init-success\n', b'init-rejected\n', b'slash-priority:',
           b'class-wildcard\n', b'class-indexed\n', b'class-late\n',
           b'completed:text-edges\n', b'completed:numeric-edges\n',
           b'completed:nebula-full-source\n')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, output, label, *, cwd=ROOT, env=None):
    with (output / f'{label}.stdout.log').open('wb') as stdout, \
         (output / f'{label}.stderr.log').open('wb') as stderr:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stdout,
                                stderr=stderr, timeout=300, check=False)
    if result.returncode:
        raise RuntimeError(f'{label} exited {result.returncode}; inspect {output}')


def dependencies(source, output, label, translation_unit=FIXTURE):
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM']
    if translation_unit == FIXTURE:
        command.append(f'-DA2FO_FIXTURE_SOURCE="{source}"')
    command.append(str(translation_unit))
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=60, check=True)
    (output / f'{label}-dependencies.txt').write_bytes(result.stdout)
    entries = result.stdout.decode().replace('\\\n', ' ').split(':', 1)[1].split()
    return sorted({str(Path(entry).resolve().relative_to(ROOT))
                   for entry in entries
                   if Path(entry).resolve().is_relative_to(ROOT)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output.exists() or output.parent != SLICE or not output.name.startswith('fixtures'):
        parser.error('output must be a new fixtures* child of the NebulaRenderer slice')
    candidate = args.rust_lib.resolve(strict=True)
    stage8_pins = json.loads((STAGE8 / 'before-hashes.json').read_text())
    checkpoint_pins = json.loads((ROOT / 'build/rust-migration-stage10/fire-arcs/checkpoint-v2/source-hashes.json').read_text())
    frozen = STAGE8 / 'before' / UNIT
    frozen_aux = STAGE8 / 'before' / AUX
    for unit, path in ((UNIT, frozen), (AUX, frozen_aux), (BRIDGE, ROOT / BRIDGE)):
        if sha(path) != stage8_pins[str(unit)]:
            raise RuntimeError(f'frozen dependency drift: {unit}')
    if sha(ORIGINAL_ARCHIVE) != ORIGINAL_SHA:
        raise RuntimeError('original Rust archive drift')
    if sha(ROOT / AUX) != stage8_pins[str(AUX)]:
        raise RuntimeError('current auxiliary source drift from frozen original')
    output.mkdir(parents=True)
    paths = {'checker': Path(__file__), 'fixture': FIXTURE,
             'old_fixture': OLD_FIXTURE, 'mock': MOCK,
             'frozen_source': frozen, 'current_source': ROOT / UNIT,
             'frozen_aux': frozen_aux, 'current_aux': ROOT / AUX,
             'bridge': ROOT / BRIDGE, 'original_archive': ORIGINAL_ARCHIVE,
             'candidate_archive': candidate,
             'frozen_manifest': STAGE8 / 'before-hashes.json',
             'checkpoint_manifest': ROOT / 'build/rust-migration-stage10/fire-arcs/checkpoint-v2/source-hashes.json',
             'rust_lib_source': ROOT / 'rust/a2fo_core/src/lib.rs'}
    for path in ROOT.glob('rust/a2fo_core/src/stage10_nebula*.rs'):
        paths[str(path.relative_to(ROOT))] = path
    for path in ROOT.glob('rust/stage10_nebula*.h'):
        paths[str(path.relative_to(ROOT))] = path
    project_dependencies = {}
    for label, source in (('before', frozen), ('current', ROOT / UNIT)):
        deps = dependencies(source, output, label)
        project_dependencies[label] = deps
        for dep in deps:
            paths.setdefault(f'dependency:{dep}', ROOT / dep)
    for label, aux in (('aux-before', frozen_aux), ('aux-current', ROOT / AUX)):
        deps = dependencies(aux, output, label, aux)
        project_dependencies[label] = deps
        for dep in deps:
            paths.setdefault(f'dependency:{dep}', ROOT / dep)
    initial = {name: sha(path) for name, path in paths.items()}
    source_used = output / 'source-used'
    for name, path in paths.items():
        if name in ('original_archive', 'candidate_archive'):
            continue
        target = source_used / name / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        if sha(target) != initial[name]:
            raise RuntimeError(f'source snapshot drift: {name}')
    for label, source in (('before', frozen), ('current', ROOT / UNIT),
                          ('aux-before', frozen_aux), ('aux-current', ROOT / AUX)):
        deps = project_dependencies[label]
        for dep in deps:
            dep_path = ROOT / dep
            if dep_path in (FIXTURE, OLD_FIXTURE, source):
                continue
            if dep.startswith('rust/stage10_nebula'):
                if sha(dep_path) != initial.get(f'dependency:{dep}'):
                    raise RuntimeError(f'candidate header drift: {dep}')
                continue
            key = dep.removeprefix('build/rust-migration-stage8/before/')
            pins = stage8_pins if label in ('before', 'aux-before', 'aux-current') else checkpoint_pins
            if pins.get(key) != sha(dep_path):
                raise RuntimeError(f'unverified {label} dependency: {dep}')
    binaries = {}
    run(['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
         '-Werror', '-shared', '-static', '-static-libgcc', '-static-libstdc++',
         str(MOCK), '-o', str(output / 'A2FOExtensions.dll')], output, 'build-mock')
    binaries['A2FOExtensions.dll'] = sha(output / 'A2FOExtensions.dll')
    for label, source, aux, lib in (
        ('before', frozen, frozen_aux, ORIGINAL_ARCHIVE),
        ('current', ROOT / UNIT, ROOT / AUX, candidate)):
        exe = output / f'{label}.exe'
        run(['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
             '-Werror', '-DNDEBUG', '-static', '-static-libgcc',
             '-static-libstdc++', f'-DA2FO_FIXTURE_SOURCE="{source}"',
             str(FIXTURE), str(aux), str(ROOT / BRIDGE), str(lib),
             '-o', str(exe)], output, f'build-{label}')
        binaries[exe.name] = sha(exe)
    receipt = {'inputs': initial, 'dependencies': project_dependencies,
               'binaries': binaries, 'original_archive_sha256': ORIGINAL_SHA,
               'required_markers': [x.decode().strip() for x in MARKERS]}
    def verify():
        for name, path in paths.items():
            if sha(path) != initial[name]:
                raise RuntimeError(f'input drift: {name}')
        for name, expected in binaries.items():
            if sha(output / name) != expected:
                raise RuntimeError(f'binary drift: {name}')
        for label, deps in project_dependencies.items():
            for dep in deps:
                path = ROOT / dep
                if path in (FIXTURE, OLD_FIXTURE, frozen, ROOT / UNIT):
                    continue
                key = dep.removeprefix('build/rust-migration-stage8/before/')
                pins = stage8_pins if label in ('before', 'aux-before', 'aux-current') else checkpoint_pins
                expected = initial.get(dep) if dep.startswith('rust/stage10_nebula') else pins.get(key)
                if expected is None or sha(path) != expected:
                    raise RuntimeError(f'dependency drift: {label}/{dep}')
    verify()
    (output / 'inputs.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if args.build_only:
        print(f'PASS built original/current full-source NebulaRenderer: {output}')
        return
    if args.wine_prefix is None:
        parser.error('--wine-prefix required for execution')
    prefix = args.wine_prefix.resolve()
    if not (prefix / 'system.reg').is_file() or not (prefix / 'drive_c').is_dir():
        parser.error('initialized private Wine prefix required')
    env = dict(os.environ)
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
               LIBGL_ALWAYS_SOFTWARE='1',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    traces = {}
    for label in ('before', 'current'):
        verify()
        case = output / 'case-filesystem'
        if case.exists():
            shutil.rmtree(case)
        case.mkdir()
        shutil.copy2(output / 'A2FOExtensions.dll', case / 'A2FOExtensions.dll')
        run(['wine', str(output / f'{label}.exe')], output, label,
            cwd=case, env=env)
        raw = (output / f'{label}.stdout.log').read_bytes()
        normalized = raw.replace(b'\r\n', b'\n')
        if not normalized or any(marker not in normalized for marker in MARKERS):
            raise RuntimeError(f'incomplete {label} trace')
        traces[label] = normalized
        (output / f'{label}.normalized.txt').write_bytes(normalized)
        verify()
    receipt['logs'] = {label: {'raw': sha(output / f'{label}.stdout.log'),
                               'stderr': sha(output / f'{label}.stderr.log'),
                               'normalized': sha(output / f'{label}.normalized.txt')}
                       for label in traces}
    receipt['equal'] = traces['before'] == traces['current']
    (output / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if not receipt['equal']:
        raise RuntimeError('NebulaRenderer original/current full-source trace differs')
    print(f'PASS NebulaRenderer full-source parity: {len(traces["before"].splitlines())} lines')


if __name__ == '__main__':
    main()
