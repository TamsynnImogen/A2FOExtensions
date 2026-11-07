#!/usr/bin/env python3
"""Pin, build and compare frozen/current full-source EnergySystems native traces."""

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/energy-systems'
STAGE8 = ROOT / 'build/rust-migration-stage8'
UNIT = Path('modules/A2FOEnergySystems/module.cpp')
FROZEN = STAGE8 / 'before' / UNIT
TEST = ROOT / 'tests/rust_stage10_energy_native.cpp'
INHERITED_TEST = ROOT / 'tests/rust_stage8_a_energy_native.cpp'
ORIGINAL = ROOT / 'build/rust-migration-stage10/nebula-renderer/checkpoint/liba2fo_core-original-win.a'
ORIGINAL_SHA = '01258fbad72411f37bebabffc2efb3e70aaeb8d7b28cf2a6f32a0558f2357c36'
ADAPTERS = (ROOT / 'modules/A2FOEnergySystems/energy_systems.cpp',
            ROOT / 'modules/A2FOEnergySystems/resupply_policy.cpp')
STAGE8_MODES = (
    'runtime', 'runtime-pc24', 'runtime-pc53', 'runtime-pc64',
    'init-short', 'init-no-armada', 'init-module-throw', 'init-signature',
    'init-shot-signature', 'init-class-false', 'init-class-throw',
    'init-weapon-false', 'init-weapon-throw', 'init-craft-false',
    'init-craft-throw', 'init-trigger-false', 'init-trigger-legacy',
    'init-alias-false', 'init-hook-false', 'init-hook-throw',
    'init-persistence-false', 'init-persistence-signature', 'init-success',
    'parse-duplicate', 'parse-null-empty', 'parse-invalid-view',
    'parse-suffix', 'parse-mode-fraction', 'parse-bool-string',
    'parse-auto-provider', 'parse-nan',
    'shot-allow', 'shot-empty', 'shot-native-zero', 'shot-disabled',
    'shot-commit-only', 'shot-cannon', 'shot-cannon-position',
    'shot-resupply-allow', 'shot-resupply-deny', 'shot-resupply-position',
    'save-normal', 'save-false', 'save-disabled',
    'load-current', 'load-legacy', 'load-invalid', 'load-missing',
    'load-native-false', 'ui-configured', 'ui-auto', 'ui-null-db', 'ui-disabled',
)
NUMERIC_MODES = tuple(f'residual-numeric-{pc}-{rc}'
                      for pc in ('pc24', 'pc53', 'pc64')
                      for rc in ('near', 'down', 'up', 'chop'))
MODES = STAGE8_MODES + NUMERIC_MODES + ('residual-ui', 'residual-ui-default')
FLAVORS = ('standalone', 'builtin')
EXPECTED_TERMINATION = {'init-hook-throw'}


def sha(path):
    hasher = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            hasher.update(block)
    return hasher.hexdigest()


def execute(command, stdout_path, stderr_path, timeout=300, env=None):
    with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                    stderr=stderr, timeout=timeout, check=False)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f'timed out: {command[0]}; logs at {stdout_path}') from error
    return result.returncode


def dependencies(translation_unit, source=None):
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
               '-I', str(ROOT / 'tests')]
    if source is not None:
        command.append(f'-DMODULE_SOURCE="{source}"')
    command.append(str(translation_unit))
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=60, check=True)
    entries = result.stdout.decode().replace('\\\n', ' ').split(':', 1)[1].split()
    return sorted({str(Path(entry).resolve().relative_to(ROOT))
                   for entry in entries
                   if Path(entry).resolve().is_relative_to(ROOT)})


def marker(mode):
    if mode == 'init-hook-throw':
        return b'hook callback'
    if mode.startswith('runtime'):
        return b'amounts:'
    if mode.startswith('init-'):
        return b'state:'
    if mode.startswith('parse-'):
        return b'weapon:'
    if mode.startswith('shot-'):
        return b'shot-state:'
    if mode.startswith(('save-', 'load-')):
        return b'stores:'
    if mode.startswith('ui-'):
        return b'ui-state:'
    if mode.startswith('residual-numeric-'):
        return f'completed:residual-numeric:{mode}'.encode()
    return f'completed:{mode}'.encode()


def validate(mode, code, stdout, stderr):
    if stdout.count(f'ready:{mode}\n'.encode()) != 1:
        raise RuntimeError(f'{mode}: missing unique readiness marker')
    if mode in EXPECTED_TERMINATION:
        if code == 0 or marker(mode) not in stderr:
            raise RuntimeError(f'{mode}: expected hook exception termination')
    elif code != 0 or marker(mode) not in stdout:
        raise RuntimeError(f'{mode}: missing successful completion marker (exit {code})')
    if mode not in EXPECTED_TERMINATION:
        completed = (f'completed:inherited:{mode}\n' if mode in STAGE8_MODES
                     else f'completed:residual-numeric:{mode}\n'
                     if mode.startswith('residual-numeric-')
                     else f'completed:{mode}\n').encode()
        if stdout.count(completed) != 1:
            raise RuntimeError(f'{mode}: missing unique completion marker')
    if mode.startswith('residual-numeric-'):
        rows = [line for line in stdout.splitlines() if b' whole ' in line]
        if len(rows) != 17 or any(not line.startswith(mode.encode() + b' whole ')
                                  for line in rows):
            raise RuntimeError(f'{mode}: expected 17 indexed numeric rows')
        for category, count in ((b' age ', 15), (b' add ', 15)):
            if sum(category in line for line in stdout.splitlines()) != count:
                raise RuntimeError(f'{mode}: expected {count} {category!r} rows')
        if sum(b' colour ' in line for line in stdout.splitlines()) != 7:
            raise RuntimeError(f'{mode}: expected seven colour rows')
    if mode == 'residual-ui' and len(stdout.splitlines()) < 10:
        raise RuntimeError('residual-ui: incomplete UI rows')
    if mode == 'residual-ui-default' and sum(
            line.startswith(b'draw:') for line in stdout.splitlines()) != 3:
        raise RuntimeError('residual-ui-default: expected three fallback draws')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--rust-lib', type=Path, required=True)
    parser.add_argument('--before-rust-lib', type=Path, default=ORIGINAL)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('--work-dir must be a new fixtures* child of the energy-systems slice')
    candidate = args.rust_lib.resolve(strict=True)
    original = args.before_rust_lib.resolve(strict=True)
    manifest = STAGE8 / 'before-hashes.json'
    pins = json.loads(manifest.read_text())
    if sha(FROZEN) != pins[str(UNIT)] or sha(original) != ORIGINAL_SHA:
        raise RuntimeError('frozen source or independent original archive drift')
    for path in ADAPTERS:
        if sha(path) != pins[str(path.relative_to(ROOT))]:
            raise RuntimeError(f'original native adapter drift: {path}')
    work.mkdir(parents=True)
    paths = {'checker': Path(__file__).resolve(), 'test': TEST,
             'inherited_test': INHERITED_TEST, 'manifest': manifest,
             'frozen_source': FROZEN, 'current_source': ROOT / UNIT,
             'original_archive': original, 'candidate_archive': candidate}
    deps = {}
    for label, translation_unit, source in (
            ('before', TEST, FROZEN), ('current', TEST, ROOT / UNIT),
            ('energy-adapter', ADAPTERS[0], None),
            ('resupply-adapter', ADAPTERS[1], None)):
        deps[label] = dependencies(translation_unit, source)
        (work / f'{label}-dependencies.txt').write_text('\n'.join(deps[label]) + '\n')
        for entry in deps[label]:
            paths.setdefault(f'dependency:{entry}', ROOT / entry)
    for path in ADAPTERS:
        paths[str(path.relative_to(ROOT))] = path
    for relative in (
            'rust/a2fo_core/src/lib.rs', 'rust/a2fo_core/src/energy.rs',
            'rust/a2fo_core/src/resupply.rs',
            'rust/a2fo_core/src/stage8_a_energy.rs',
            'rust/a2fo_core/src/stage9_energy_numeric.rs',
            'rust/a2fo_core/src/stage10_energy_runtime.rs',
            'rust/stage8_a_energy_ffi.h', 'rust/stage9_energy_numeric_ffi.h',
            'rust/stage10_energy_runtime_ffi.h'):
        candidate_path = ROOT / relative
        if candidate_path.is_file():
            paths[f'rust-source:{relative}'] = candidate_path
    frozen_prefix = 'build/rust-migration-stage8/before/'
    for entry in deps['before']:
        if entry in (str(TEST.relative_to(ROOT)), str(INHERITED_TEST.relative_to(ROOT))):
            continue
        logical = entry.removeprefix(frozen_prefix)
        if logical not in pins or sha(ROOT / entry) != pins[logical]:
            raise RuntimeError(f'unverified frozen dependency: {entry}')
    for entry in deps['current']:
        if entry in (str(TEST.relative_to(ROOT)), str(INHERITED_TEST.relative_to(ROOT)),
                     str(UNIT)):
            continue
        if entry in pins and sha(ROOT / entry) != pins[entry]:
            raise RuntimeError(f'original header/adapter drift: {entry}')
    for label in ('energy-adapter', 'resupply-adapter'):
        for entry in deps[label]:
            if entry not in pins or sha(ROOT / entry) != pins[entry]:
                raise RuntimeError(f'unverified original {label} dependency: {entry}')
    initial = {name: sha(path) for name, path in paths.items()}
    for name, path in paths.items():
        if name.endswith('archive'):
            continue
        target = work / 'source-used' / name / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        if sha(target) != initial[name]:
            raise RuntimeError(f'source snapshot mismatch: {name}')

    def verify():
        changed = [name for name, path in paths.items() if sha(path) != initial[name]]
        if changed:
            raise RuntimeError('input drift: ' + ', '.join(changed))

    binaries = {}
    receipt = {'status': 'building', 'inputs_before': initial,
               'dependencies': deps, 'binaries': binaries,
               'commands': [],
               'modes': list(MODES), 'flavors': list(FLAVORS),
               'expected_trace_pairs': len(MODES) * len(FLAVORS),
               'assertions_enabled': True,
               'fixture_assertion_sites': TEST.read_text().count('assert(') +
                                          INHERITED_TEST.read_text().count('assert(')}
    (work / 'inputs.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    for flavor in FLAVORS:
        for label, source, archive in (('before', FROZEN, original),
                                       ('current', ROOT / UNIT, candidate)):
            exe = work / f'{flavor}-{label}.exe'
            command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
                       '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
                       '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                       '-static', '-I', str(ROOT / 'tests'),
                       f'-DMODULE_SOURCE="{source}"']
            if flavor == 'builtin':
                command.append('-DA2FO_BUILTIN_STARTUP=1')
            command += [str(TEST), *(str(path) for path in ADAPTERS),
                        str(archive), '-lws2_32', '-lbcrypt', '-lole32',
                        '-ladvapi32', '-o', str(exe)]
            code = execute(command, work / f'build-{flavor}-{label}.stdout.log',
                           work / f'build-{flavor}-{label}.stderr.log')
            receipt['commands'].append({
                'kind': 'compile', 'flavor': flavor, 'version': label,
                'argv': command, 'exit_code': code,
                'stdout_sha256': sha(work / f'build-{flavor}-{label}.stdout.log'),
                'stderr_sha256': sha(work / f'build-{flavor}-{label}.stderr.log')})
            if code:
                receipt.update(status='failed', error=f'compile failed: {flavor}/{label}',
                               inputs_after={name: sha(path) for name, path in paths.items()})
                (work / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
                raise RuntimeError(f'compile failed: {flavor}/{label}; see build log')
            binaries[f'{flavor}-{label}'] = sha(exe)
            verify()
    receipt['status'] = 'built' if args.build_only else 'running'
    (work / 'inputs.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    if args.build_only:
        receipt['inputs_after'] = {name: sha(path) for name, path in paths.items()}
        (work / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        print(f'PASS built frozen/current EnergySystems standalone+builtin: {work}')
        return 0
    if args.wine_prefix is None:
        parser.error('--wine-prefix required for native execution')
    prefix = args.wine_prefix.resolve()
    if not (prefix / 'system.reg').is_file() or not (prefix / 'drive_c').is_dir():
        parser.error('initialized private Wine prefix required')
    env = dict(os.environ)
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                   'winewayland.drv=d;winex11.drv=d')
    traces = {}
    try:
        for flavor in FLAVORS:
            for mode in MODES:
                pair = {}
                for label in ('before', 'current'):
                    verify()
                    exe = work / f'{flavor}-{label}.exe'
                    if sha(exe) != binaries[f'{flavor}-{label}']:
                        raise RuntimeError(f'binary drift: {flavor}/{label}')
                    prefix_name = f'{flavor}-{mode}-{label}'
                    stdout_path = work / f'{prefix_name}.stdout.log'
                    stderr_path = work / f'{prefix_name}.stderr.log'
                    code = execute(['wine', str(exe), mode], stdout_path,
                                   stderr_path, timeout=180, env=env)
                    receipt['commands'].append({
                        'kind': 'run', 'flavor': flavor, 'version': label,
                        'mode': mode, 'argv': ['wine', str(exe), mode],
                        'exit_code': code, 'stdout_sha256': sha(stdout_path),
                        'stderr_sha256': sha(stderr_path)})
                    stdout = stdout_path.read_bytes().replace(b'\r\n', b'\n')
                    stderr = stderr_path.read_bytes().replace(b'\r\n', b'\n')
                    validate(mode, code, stdout, stderr)
                    pair[label] = (code, stdout, stderr)
                if pair['before'] != pair['current']:
                    left, right = pair['before'], pair['current']
                    diff = difflib.unified_diff(
                        (f'exit:{left[0]}\nstdout:\n'.encode() + left[1] + b'stderr:\n' + left[2]).decode(errors='replace').splitlines(True),
                        (f'exit:{right[0]}\nstdout:\n'.encode() + right[1] + b'stderr:\n' + right[2]).decode(errors='replace').splitlines(True),
                        fromfile='before', tofile='current')
                    (work / 'firstdiff.txt').write_text(''.join(diff))
                    raise RuntimeError(f'trace mismatch: {flavor}/{mode}')
                traces[f'{flavor}/{mode}'] = {
                    'exit': pair['before'][0], 'stdout_sha256': hashlib.sha256(pair['before'][1]).hexdigest(),
                    'stderr_sha256': hashlib.sha256(pair['before'][2]).hexdigest(),
                    'stdout_rows': len(pair['before'][1].splitlines())}
        verify()
        receipt.update(status='passed', traces=traces,
                       inputs_after={name: sha(path) for name, path in paths.items()})
    except Exception as error:
        receipt.update(status='failed', error=str(error),
                       inputs_after={name: sha(path) for name, path in paths.items()})
        print(error, file=sys.stderr)
        return_code = 1
    else:
        return_code = 0
    (work / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    print(f'EnergySystems full-source native parity: {receipt["status"]}')
    return return_code


if __name__ == '__main__':
    sys.exit(main())
