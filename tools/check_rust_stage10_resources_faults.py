#!/usr/bin/env python3
"""Build and compare isolated frozen/current Resources native allocation faults."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/resources'
STAGE8 = ROOT / 'build/rust-migration-stage8'
UNIT = Path('modules/A2FOResources/module.cpp')
ADAPTER = Path('modules/A2FOResources/additional_resources.cpp')
FROZEN = STAGE8 / 'before' / UNIT
ORIGINAL = ROOT / 'build/rust-migration-stage10/energy-systems/checkpoint/liba2fo_core-original-win.a'
ORIGINAL_SHA = '01258fbad72411f37bebabffc2efb3e70aaeb8d7b28cf2a6f32a0558f2357c36'
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
FIXTURE = ROOT / 'tests/rust_stage10_resources_faults.cpp'
CASES = ('set', 'refund', 'class', 'race', 'commit')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(argv, work, label, *, cwd=ROOT, env=None):
    stdout = work / f'{label}.stdout.log'
    stderr = work / f'{label}.stderr.log'
    with stdout.open('wb') as out, stderr.open('wb') as err:
        result = subprocess.run(argv, cwd=cwd, env=env, stdout=out, stderr=err,
                                timeout=300, check=False)
    return result.returncode


def dependencies(source, adapter, work, label):
    argv = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
            f'-DMODULE_SOURCE="{source}"', str(FIXTURE), str(adapter)]
    code = execute(argv, work, f'deps-{label}')
    if code:
        raise RuntimeError(f'{label} dependency scan exit {code}')
    output = (work / f'deps-{label}.stdout.log').read_text()
    entries = set()
    for token in output.replace('\\\n', ' ').split():
        candidate = Path(token.rstrip(':'))
        if candidate.is_file() and candidate.resolve().is_relative_to(ROOT):
            entries.add(candidate.resolve())
    return sorted(entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--rust-lib', type=Path, default=CANDIDATE)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('faults'):
        parser.error('--work-dir must be a fresh faults* child of the Resources slice')
    if not args.build_only and args.wine_prefix is None:
        parser.error('--wine-prefix is required for execution')
    candidate = args.rust_lib.resolve(strict=True)
    pins = json.loads((STAGE8 / 'before-hashes.json').read_text())
    if sha(FROZEN) != pins[str(UNIT)] or sha(ORIGINAL) != ORIGINAL_SHA:
        raise RuntimeError('frozen source or original archive pin drift')
    frozen_adapter = STAGE8 / 'before' / ADAPTER
    if sha(frozen_adapter) != pins[str(ADAPTER)] or sha(ROOT / ADAPTER) != pins[str(ADAPTER)]:
        raise RuntimeError('real additional_resources.cpp adapter pin drift')
    work.mkdir(parents=True)
    receipt = {'status': 'preparing', 'commands': [], 'inputs': {},
               'binaries': {}, 'outcomes': {}}
    def save():
        (work / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    try:
        inputs = {'checker': Path(__file__).resolve(), 'fixture': FIXTURE,
                  'frozen_source': FROZEN, 'current_source': ROOT / UNIT,
                  'frozen_adapter': frozen_adapter, 'current_adapter': ROOT / ADAPTER,
                  'frozen_manifest': STAGE8 / 'before-hashes.json',
                  'original_archive': ORIGINAL, 'candidate_archive': candidate}
        for version, source, adapter in (('before', FROZEN, frozen_adapter),
                                         ('current', ROOT / UNIT, ROOT / ADAPTER)):
            for dep in dependencies(source, adapter, work, version):
                if version == 'before' and dep.is_relative_to(STAGE8 / 'before'):
                    key = str(dep.relative_to(STAGE8 / 'before'))
                    if key not in pins or sha(dep) != pins[key]:
                        raise RuntimeError(f'frozen dependency pin drift: {key}')
                inputs.setdefault(f'dependency:{dep.relative_to(ROOT)}', dep)
        receipt['inputs'] = {key: sha(path) for key, path in inputs.items()}
        for name, path in inputs.items():
            if name.endswith('archive'):
                continue
            target = work / 'source-used' / name / path.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if sha(target) != receipt['inputs'][name]:
                raise RuntimeError(f'snapshot drift: {name}')
        for version, source, adapter, archive in (
            ('before', FROZEN, frozen_adapter, ORIGINAL),
            ('current', ROOT / UNIT, ROOT / ADAPTER, candidate),
        ):
            exe = work / f'{version}.exe'
            argv = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
                    '-Wextra', '-Werror', '-Wno-unused-function', '-UNDEBUG', '-ffunction-sections',
                    '-fdata-sections', '-Wl,--gc-sections', '-static',
                    f'-DMODULE_SOURCE="{source}"', str(FIXTURE), str(adapter),
                    str(archive), '-o', str(exe)]
            code = execute(argv, work, f'build-{version}')
            receipt['commands'].append({'label': f'build-{version}', 'argv': argv,
                                        'exit_code': code})
            if code:
                raise RuntimeError(f'{version} build exit {code}')
            receipt['binaries'][version] = sha(exe)
        def verify_inputs():
            for name, path in inputs.items():
                if sha(path) != receipt['inputs'][name]:
                    raise RuntimeError(f'input changed during check: {name}')
            for version in ('before', 'current'):
                if sha(work / f'{version}.exe') != receipt['binaries'][version]:
                    raise RuntimeError(f'binary changed during check: {version}')
        verify_inputs()
        if args.build_only:
            receipt['status'] = 'built'
            save()
            print(f'PASS Resources faults compile-only: {work}')
            return
        prefix = args.wine_prefix.resolve()
        if not (prefix / 'system.reg').is_file() or not (prefix / 'drive_c').is_dir():
            raise RuntimeError('initialized private Wine prefix required')
        env = dict(os.environ)
        env.pop('DISPLAY', None); env.pop('WAYLAND_DISPLAY', None)
        env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                      'winewayland.drv=d;winex11.drv=d')
        for case in CASES:
            traces = {}
            for version in ('before', 'current'):
                verify_inputs()
                argv = ['wine', str(work / f'{version}.exe'), case]
                label = f'run-{case}-{version}'
                code = execute(argv, work, label, env=env)
                receipt['commands'].append({'label': label, 'argv': argv,
                                            'exit_code': code})
                expected_code = 77 if case == 'commit' else 0
                if code != expected_code:
                    raise RuntimeError(f'{label} exit {code}, expected {expected_code}')
                trace = (work / f'{label}.stdout.log').read_text().strip()
                pattern = (rf'case:{case}:terminate=1:failure=1:depth=\d+:enters=\d+:'
                           rf'leaves=\d+:leave_uncaught=-?\d+:unwind_leaves=\d+:'
                           rf'uncaught=\d+' if case == 'commit' else
                           rf'case:{case}:bad_alloc=1:failure=1:depth=0:'
                           rf'enters=(\d+):leaves=\1:leave_uncaught=[1-9]\d*:'
                           rf'unwind_leaves=[1-9]\d*')
                if not re.fullmatch(pattern, trace):
                    raise RuntimeError(f'{label} unexpected trace: {trace!r}')
                traces[version] = trace
                receipt['outcomes'][label] = {'exit_code': code, 'trace': trace,
                    'stdout_sha256': sha(work / f'{label}.stdout.log'),
                    'stderr_sha256': sha(work / f'{label}.stderr.log')}
            if traces['before'] != traces['current']:
                raise RuntimeError(f'{case} frozen/current mismatch: {traces}')
        verify_inputs()
        receipt['status'] = 'passed'
        save()
        print(f'PASS Resources native fault parity: {len(CASES)} cases')
    except Exception as error:
        receipt['status'] = 'failed'; receipt['error'] = str(error); save()
        raise


if __name__ == '__main__':
    main()
