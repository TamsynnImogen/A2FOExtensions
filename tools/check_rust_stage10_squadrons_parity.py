#!/usr/bin/env python3
"""Build and compare original/current full-source Squadrons native traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from check_rust_stage8_a_squadrons_parity import MODES as INHERITED_MODES, FLAVORS


ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/squadrons'
STAGE8 = ROOT / 'build/rust-migration-stage8'
UNIT = Path('modules/A2FOSquadrons/module.cpp')
FROZEN = STAGE8 / 'before' / UNIT
FIXTURE = ROOT / 'tests/rust_stage10_squadrons_native.cpp'
INHERITED = ROOT / 'tests/rust_stage8_a_squadrons_native.cpp'
ORIGINAL = ROOT / 'build/rust-migration-stage10/resources/checkpoint/liba2fo_core-original-win.a'
ORIGINAL_SHA = '01258fbad72411f37bebabffc2efb3e70aaeb8d7b28cf2a6f32a0558f2357c36'
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
ADAPTER_NAMES = ('squadron_config.cpp', 'squadron_state.cpp', 'thiscall_bridge.S')
BEFORE_ADAPTERS = tuple(STAGE8 / 'before/modules/A2FOSquadrons' / name
                        for name in ADAPTER_NAMES)
CURRENT_ADAPTERS = tuple(ROOT / 'modules/A2FOSquadrons' / name
                         for name in ADAPTER_NAMES)
INLINES = tuple(Path(f'modules/A2FOSquadrons/{name}') for name in
                ('native_pod_selection.inl', 'native_repair.inl',
                 'native_economics.inl'))
EXPECTED_TERMINATION = {'init-hook-throw'}
NUMERIC_MODES = tuple(f'numeric-{pc}-{rc}'
                      for pc in ('pc24', 'pc53', 'pc64')
                      for rc in ('rcnear', 'rcdown', 'rcup', 'rcchop'))
MODES = INHERITED_MODES + NUMERIC_MODES + ('costs-parse', 'bar-reentry')


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def run(command, stdout_path, stderr_path, env=None, timeout=300):
    with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                    stderr=stderr, timeout=timeout, check=False)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f'timed out: {command[0]}; see {stdout_path}') from error
    return result.returncode


def dependencies(unit, source=None):
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
               '-I', str(ROOT / 'tests')]
    if source is not None:
        command.append(f'-DMODULE_SOURCE="{source}"')
    command.append(str(unit))
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=60, check=True)
    words = result.stdout.decode().replace('\\\n', ' ').split(':', 1)[1].split()
    return sorted({path.relative_to(ROOT).as_posix()
                   for word in words if (path := Path(word).resolve()).is_relative_to(ROOT)})


def validate(mode, code, stdout, stderr):
    ready = f'ready:{mode}\n'.encode()
    completed = (f'completed:numeric:{mode}\n' if mode in NUMERIC_MODES
                 else f'completed:{mode}\n' if mode in ('costs-parse', 'bar-reentry')
                 else f'completed:inherited:{mode}\n').encode()
    if stdout.count(ready) != 1:
        raise RuntimeError(f'{mode}: missing unique readiness marker')
    if mode in EXPECTED_TERMINATION:
        if code != 86 or b'terminate-active:hook injected' not in stderr:
            raise RuntimeError(f'{mode}: missing expected native termination')
        if completed in stdout:
            raise RuntimeError(f'{mode}: unexpected completion after termination')
    elif code or stdout.count(completed) != 1 or len(stdout.splitlines()) < 3:
        raise RuntimeError(f'{mode}: missing successful trace/completion, exit {code}')
    if mode.startswith('bar-pc'):
        if b'bar:' not in stdout or b'colour:' not in stdout:
            raise RuntimeError(f'{mode}: missing numeric bar output')
    if mode.startswith('econ-pc'):
        if b'class:' not in stdout:
            raise RuntimeError(f'{mode}: missing economics output')
    if mode == 'selection-mutation' and b'mutation:registry-peer-removed' not in stdout:
        raise RuntimeError(f'{mode}: missing mutation')
    if mode == 'econ-reentrant' and b'mutation:class-reloaded' not in stdout:
        raise RuntimeError(f'{mode}: missing reentry')
    if mode in NUMERIC_MODES:
        if (stdout.count(f'numeric:{mode}:cw='.encode()) != 1 or
                b'class:' not in stdout or b'bar:' not in stdout or
                b'colour:' not in stdout or
                b'value-reentry:1' not in stdout or
                stdout.count(b'value-order:') != 2 or
                stdout.count(b'colour-order:') != 2):
            raise RuntimeError(f'{mode}: incomplete x87/economics/bar trace')
        for prefix, expected in ((b'matrix:', 26), (b'near:', 44),
                                 (b'retry:', 14)):
            rows = [line for line in stdout.splitlines()
                    if line.startswith(prefix + mode.encode() + b':')]
            if len(rows) != expected:
                raise RuntimeError(f'{mode}: expected {expected} {prefix!r} rows')
    if mode == 'costs-parse' and sum(
            line.startswith(b'cost:') for line in stdout.splitlines()) != 9:
        raise RuntimeError('costs-parse: expected nine parser rows')
    if mode == 'bar-reentry':
        for marker in (b'samebar:outer:1:11', b'samebar:outer:2:12',
                       b'samebar:nested:1:11', b'samebar:nested:2:12',
                       b'samebar:restore-peer:1', b'samebar:result:'):
            if stdout.count(marker) != 1:
                raise RuntimeError(f'bar-reentry: missing unique {marker!r}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--rust-lib', type=Path, default=CANDIDATE)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('--work-dir must be a fresh fixtures* child of the Squadrons slice')
    candidate = args.rust_lib.resolve(strict=True)
    manifest = STAGE8 / 'before-hashes.json'
    pins = json.loads(manifest.read_text())
    if sha(ORIGINAL) != ORIGINAL_SHA:
        raise RuntimeError('independent original archive drift')
    for logical in (UNIT, *INLINES):
        if sha(STAGE8 / 'before' / logical) != pins[str(logical)]:
            raise RuntimeError(f'frozen native source drift: {logical}')
    for adapter in BEFORE_ADAPTERS:
        logical = adapter.relative_to(STAGE8 / 'before').as_posix()
        if sha(adapter) != pins[logical]:
            raise RuntimeError(f'original native adapter drift: {logical}')
    work.mkdir(parents=True)
    paths = {'checker': Path(__file__).resolve(),
             'inherited_checker': ROOT / 'tools/check_rust_stage8_a_squadrons_parity.py',
             'fixture': FIXTURE,
             'inherited_fixture': INHERITED, 'frozen_manifest': manifest,
             'stage10_manifest': SLICE / 'before-hashes.json',
             'original_archive': ORIGINAL, 'candidate_archive': candidate,
             'frozen_source': FROZEN, 'current_source': ROOT / UNIT}
    for name in ('numeric-reference.cpp', 'numeric-reference.s'):
        path = SLICE / name
        if path.is_file():
            paths[name] = path
    dep_sets = {}
    for label, unit, source in (
            ('before', FIXTURE, FROZEN), ('current', FIXTURE, ROOT / UNIT),
            *[(f'before-adapter-{index}', adapter, None)
              for index, adapter in enumerate(BEFORE_ADAPTERS)],
            *[(f'current-adapter-{index}', adapter, None)
              for index, adapter in enumerate(CURRENT_ADAPTERS)]):
        deps = dependencies(unit, source)
        dep_sets[label] = deps
        (work / f'{label}-dependencies.txt').write_text('\n'.join(deps) + '\n')
        for entry in deps:
            paths.setdefault(f'dependency:{entry}', ROOT / entry)
            logical = entry.removeprefix('build/rust-migration-stage8/before/')
            if label == 'before' and entry not in (str(FIXTURE.relative_to(ROOT)),
                                                   str(INHERITED.relative_to(ROOT))):
                if logical not in pins or sha(ROOT / entry) != pins[logical]:
                    raise RuntimeError(f'unverified frozen dependency: {entry}')
            elif label.startswith('before-adapter-'):
                if logical not in pins or sha(ROOT / entry) != pins[logical]:
                    raise RuntimeError(f'unverified original adapter dependency: {entry}')
    for adapter in (*BEFORE_ADAPTERS, *CURRENT_ADAPTERS):
        paths[adapter.relative_to(ROOT).as_posix()] = adapter
    for pattern in ('rust/a2fo_core/src/stage8_a_squadrons.rs',
                    'rust/a2fo_core/src/stage10_squadrons*.rs',
                    'rust/a2fo_core/src/lib.rs', 'rust/stage8_a_squadrons_ffi.h',
                    'rust/stage10_squadrons*.h'):
        for path in ROOT.glob(pattern):
            paths[path.relative_to(ROOT).as_posix()] = path
    initial = {name: sha(path) for name, path in paths.items()}
    for name, path in paths.items():
        destination = work / 'source-used' / name / path.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        if sha(destination) != initial[name]:
            raise RuntimeError(f'snapshot mismatch: {name}')

    def verify():
        drift = [name for name, path in paths.items() if sha(path) != initial[name]]
        if drift:
            raise RuntimeError('input drift: ' + ', '.join(drift))

    receipt = {'status': 'building', 'inputs_before': initial,
               'dependencies': dep_sets, 'commands': [], 'binaries': {},
               'modes': list(MODES), 'flavors': list(FLAVORS),
               'expected_trace_pairs': len(MODES) * len(FLAVORS),
               'assertions_enabled': True}

    def save():
        receipt['inputs_after'] = {name: sha(path) for name, path in paths.items()}
        (work / 'receipt.json').write_text(json.dumps(receipt, indent=2,
                                                       sort_keys=True) + '\n')

    for flavor in FLAVORS:
        for version, source, archive, adapters in (
                ('before', FROZEN, ORIGINAL, BEFORE_ADAPTERS),
                ('current', ROOT / UNIT, candidate, CURRENT_ADAPTERS)):
            exe = work / f'{flavor}-{version}.exe'
            command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
                       '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
                       '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                       '-static', '-I', str(ROOT / 'tests'),
                       f'-DMODULE_SOURCE="{source}"']
            if flavor == 'builtin':
                command += ['-DA2FO_ModuleInit=A2FOBuiltinSquadrons_Init',
                            '-DA2FO_ModuleShutdown=A2FOBuiltinSquadrons_Shutdown']
            command += [str(FIXTURE), *(str(x) for x in adapters), str(archive),
                        '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32', '-o', str(exe)]
            label = f'build-{flavor}-{version}'
            stdout, stderr = work / f'{label}.stdout.log', work / f'{label}.stderr.log'
            code = run(command, stdout, stderr)
            receipt['commands'].append({'kind': 'compile', 'flavor': flavor,
                                        'version': version, 'argv': command,
                                        'exit_code': code, 'stdout_sha256': sha(stdout),
                                        'stderr_sha256': sha(stderr)})
            if code:
                receipt.update(status='failed', error=f'{label} exited {code}')
                save()
                raise RuntimeError(f'{label} failed; see {stderr}')
            receipt['binaries'][f'{flavor}-{version}'] = sha(exe)
            verify()
    if args.build_only:
        receipt['status'] = 'built'
        save()
        print(f'PASS Squadrons original/current standalone+builtin build: {work}')
        return
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
    pairs = 0
    for flavor in FLAVORS:
        for mode in MODES:
            outputs = {}
            for version in ('before', 'current'):
                verify()
                exe = work / f'{flavor}-{version}.exe'
                if sha(exe) != receipt['binaries'][f'{flavor}-{version}']:
                    raise RuntimeError(f'binary drift: {exe}')
                label = f'run-{flavor}-{mode}-{version}'
                stdout, stderr = work / f'{label}.stdout.log', work / f'{label}.stderr.log'
                command = ['wine', str(exe), mode]
                code = run(command, stdout, stderr,
                           env=env, timeout=180)
                out, err = stdout.read_bytes().replace(b'\r\n', b'\n'), \
                           stderr.read_bytes().replace(b'\r\n', b'\n')
                receipt['commands'].append({'kind': 'run', 'flavor': flavor,
                                            'mode': mode, 'version': version,
                                            'argv': command,
                                            'exit_code': code, 'stdout_sha256': sha(stdout),
                                            'stderr_sha256': sha(stderr)})
                try:
                    validate(mode, code, out, err)
                except RuntimeError as error:
                    receipt.update(status='failed', error=str(error))
                    save()
                    raise
                outputs[version] = (code, out, err)
            if outputs['before'] != outputs['current']:
                receipt.update(status='failed', error=f'trace mismatch: {flavor}/{mode}')
                save()
                raise RuntimeError(receipt['error'])
            pairs += 1
    verify()
    receipt.update(status='pass', compared_trace_pairs=pairs,
                   total_lines=sum(len((work / f'run-{flavor}-{mode}-before.stdout.log')
                                       .read_bytes().splitlines())
                                   for flavor in FLAVORS for mode in MODES))
    save()
    print(f'PASS Squadrons {pairs} original/current native trace pairs: {work}')


if __name__ == '__main__':
    main()
