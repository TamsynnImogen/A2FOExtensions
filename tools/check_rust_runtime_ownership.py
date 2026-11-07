#!/usr/bin/env python3
"""Pinned bounded stage-11 core parity, Rust-owned records, and real DLL load.

Only writes a fresh fixtures* child of runtime-ownership. The five historical
core cases exclude allocation-budget fixtures because C++/Rust allocation
mechanisms changed; they remain a separate semantic-invariant review.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import check_rust_stage10_core_completion as core

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage11/runtime-ownership'
BEFORE = SLICE / 'phase2-before'
PREVIOUS = SLICE / 'checkpoint-v5/artifacts'
ORIGINAL = PREVIOUS / 'rust/i686-pc-windows-gnu/release/liba2fo_core.a'
CANDIDATE = SLICE / 'candidate-v6/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
DLL = SLICE / 'candidate-v6/A2FOExtensions.dll'
RECEIPT = SLICE / 'build-receipt-v6.json'
PREDECESSOR_RECEIPT = SLICE / 'checkpoint-v5/build-receipt-v5.json'
PREDECESSOR_CHECKPOINT = SLICE / 'checkpoint-v5/checkpoint.json'
FROZEN_HASHES = SLICE / 'phase2-before-hashes.json'
SUITES = ('settings', 'lifecycle', 'dispatch', 'classload', 'lookup')
MARKERS = {name: core.CORE_MARKERS[name] for name in SUITES}
CXX = 'i686-w64-mingw32-g++'
FLAGS = ('-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-UNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections',
         '-fdata-sections', '-static')


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def save(work, record):
    (work / 'receipt.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')


def execute(argv, work, key, pins, cwd=ROOT, env=None, expected=0, timeout=360):
    pins.verify()
    out, err = work / f'{key}.stdout', work / f'{key}.stderr'
    with out.open('wb') as stdout, err.open('wb') as stderr:
        try:
            result = subprocess.run(argv, cwd=cwd, env=env, stdout=stdout,
                                    stderr=stderr, timeout=timeout, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    event = {'argv': list(map(str, argv)), 'cwd': str(cwd), 'exit': code,
             'stdout': str(out), 'stderr': str(err),
             'stdout_sha256': sha(out), 'stderr_sha256': sha(err)}
    (work / f'{key}.json').write_text(json.dumps(event, indent=2) + '\n')
    pins.verify()
    if code != expected:
        raise RuntimeError(f'{key}: exit {code}, expected {expected}; see {err}')
    return event


def private_rva(work, pins):
    symbol = execute(['i686-w64-mingw32-nm', str(DLL)], work, 'dll-nm', pins)
    image = execute(['i686-w64-mingw32-objdump', '-p', str(DLL)], work, 'dll-pe', pins)
    matches = re.findall(rb'^([0-9a-fA-F]+)\s+[Tt]\s+_a2fo_rs_runtime_globals$',
                         Path(symbol['stdout']).read_bytes(), re.MULTILINE)
    bases = re.findall(rb'^ImageBase\s+([0-9a-fA-F]+)$',
                       Path(image['stdout']).read_bytes(), re.MULTILINE)
    if len(matches) != 1 or len(bases) != 1:
        raise RuntimeError('unique private globals symbol and PE ImageBase required')
    address, base = int(matches[0], 16), int(bases[0], 16)
    if not base < address < base + 0x10000000:
        raise RuntimeError('private globals symbol outside plausible image range')
    return address - base, address, base


def compile_single(name, units, archive, work, pins, record, extra_flags=()):
    objects = []
    for index, unit in enumerate(units):
        pins.add(unit)
        obj = work / f'{name}-{index}.o'
        command = [CXX, *FLAGS, *extra_flags, str(unit), '-c', '-o', str(obj)]
        dependency = core.dependency_scan(command, unit, work,
                                          f'{name}-dep-{index}', pins=pins)
        for path in dependency:
            if path.is_relative_to(BEFORE):
                core.pinned_before(path, record['frozen_hashes'])
            pins.add(path)
        record['dependencies'][f'{name}-{index}'] = sorted(map(str, dependency))
        execute(command, work, f'{name}-compile-{index}', pins)
        objects.append(obj)
    exe = work / f'{name}.exe'
    command = [CXX, '-static', '-Wl,--gc-sections', *map(str, objects)]
    if archive:
        command.append(str(archive))
        command += core.CORE_LIBS + core.RUST_LIBS
    command += ['-o', str(exe)]
    execute(command, work, f'{name}-link', pins)
    record['cases'][name] = {'exe': str(exe), 'sha256': sha(exe), 'link': command}
    save(work, record)


def compile_probe(work, pins, record):
    unit = ROOT / 'tests/runtime_module_probe.cpp'
    pins.add(unit)
    obj = work / 'module-probe.o'
    command = [CXX, *FLAGS, str(unit), '-c', '-o', str(obj)]
    dependency = core.dependency_scan(command, unit, work, 'module-probe-dep',
                                      pins=pins)
    for path in dependency:
        if path.is_relative_to(BEFORE):
            core.pinned_before(path, record['frozen_hashes'])
        pins.add(path)
    record['dependencies']['module-probe'] = sorted(map(str, dependency))
    execute(command, work, 'module-probe-compile', pins)
    dll = work / 'runtime_module_probe.dll'
    link = [CXX, '-shared', '-static', '-Wl,--gc-sections', str(obj), '-o', str(dll)]
    execute(link, work, 'module-probe-link', pins)
    record['cases']['module-probe'] = {'dll': str(dll), 'sha256': sha(dll),
                                       'link': link}
    save(work, record)


def capture_runtime(runtime, work, key):
    captured = work / 'runtime-captured' / key
    captured.parent.mkdir(parents=True, exist_ok=True)
    runtime.rename(captured)
    return {p.relative_to(captured).as_posix(): sha(p)
            for p in captured.rglob('*') if p.is_file()}


def dispatch_sections(stdout):
    lines = stdout.splitlines(keepends=True)
    previous = [i for i, line in enumerate(lines)
                if line.startswith(b'class-get-throw:')]
    following = [i for i, line in enumerate(lines)
                 if line.startswith(b'trigger:')]
    if len(previous) != 1 or not following or previous[0] >= following[0]:
        raise RuntimeError('dispatch allocation case boundaries changed')
    begin, end = previous[0] + 1, following[0]
    allocation = b''.join(lines[begin:end])
    if sum(line.startswith(b'class-alloc:') for line in lines[begin:end]) != 1:
        raise RuntimeError('dispatch allocation case marker absent or repeated')
    return b''.join(lines[:begin]), allocation, b''.join(lines[end:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path('/tmp/a2fo-shields-wine-prefix'))
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('work directory must be a fresh fixtures* child of runtime-ownership')
    if not all(path.is_file() for path in (ORIGINAL, CANDIDATE, DLL, RECEIPT)):
        parser.error('immutable original, candidate, DLL and formal build receipt required')
    build = json.loads(RECEIPT.read_text())
    keys = (CANDIDATE.relative_to(ROOT).as_posix(), DLL.relative_to(ROOT).as_posix())
    if build.get('status') != 'passed' or any(
            build.get('artifacts', {}).get(key) != sha(ROOT / key) for key in keys):
        parser.error('passed formal build receipt must pin Rust archive and DLL')
    if any(sha(ROOT / logical) != digest for logical, digest in
           build['artifacts'].items()):
        parser.error('formal build artifact drift')
    current = dict(build.get('inputs_after', {}))
    if len(current) != 1304 or len(build.get('artifacts', {})) != 6:
        parser.error('formal v6 build input or artifact count changed')
    for key in ('Makefile', 'rust/a2fo_core/Cargo.toml',
                'rust/a2fo_core/Cargo.lock', 'core/dllmain.cpp',
                'core/module_loader.cpp'):
        if current.get(key) != sha(ROOT / key):
            parser.error(f'formal build lacks current product pin: {key}')
    frozen = json.loads(FROZEN_HASHES.read_text())
    if len(frozen) != 1518:
        parser.error('frozen phase-two baseline is not 1518 files')
    checkpoint = json.loads(PREDECESSOR_CHECKPOINT.read_text())
    predecessor = json.loads(PREDECESSOR_RECEIPT.read_text())
    original_key = ORIGINAL.relative_to(ROOT).as_posix().replace(
        '/checkpoint-v5/artifacts/', '/candidate-v5/')
    if (checkpoint.get('status') != 'passed-bounded' or
            checkpoint.get('build_receipt_sha256') != sha(PREDECESSOR_RECEIPT) or
            predecessor.get('status') != 'passed' or
            predecessor.get('artifacts', {}).get(original_key) != sha(ORIGINAL)):
        parser.error('original archive differs from passed v5 checkpoint')
    if not args.build_only and not (args.wine_prefix.resolve() / 'system.reg').is_file():
        parser.error('initialized exclusive private Wine prefix required')
    work.mkdir()
    record = {'status': 'preparing', 'selected_core_suites': SUITES,
              'full_historical_core_matrix': False,
              'allocation_budget_fixtures_excluded': ('odf',
                  'classdefaults', 'parameters'),
              'extra_core_traps': ('_A2FOBuiltinModule_StartupReady',
                                   '_A2FO_ProducerEventsReady'),
              'dispatch_allocation_section': {},
              'cases': {}, 'dependencies': {}, 'runs': {}, 'core_pairs': 0,
              'executions': 0, 'build_only': args.build_only,
              'build_receipt': str(RECEIPT), 'build_receipt_sha256': sha(RECEIPT),
              'independent_test_source_amendment': None,
              'expected_executions': 15, 'expected_compiled_cases': 16,
              'original_archive': str(ORIGINAL), 'original_sha256': sha(ORIGINAL),
              'candidate_archive': str(CANDIDATE), 'candidate_sha256': sha(CANDIDATE),
              'dll': str(DLL), 'dll_sha256': sha(DLL)}
    save(work, record)
    try:
        pins = core.InputPins()
        for logical, expected in frozen.items():
            pins.add(BEFORE / logical, expected)
        for logical, expected in current.items():
            pins.add(ROOT / logical, expected)
        for logical, expected in build['artifacts'].items():
            pins.add(ROOT / logical, expected)
        for path in (ORIGINAL, CANDIDATE, DLL, RECEIPT, Path(__file__),
                     Path(core.__file__), FROZEN_HASHES,
                     PREDECESSOR_RECEIPT, PREDECESSOR_CHECKPOINT,
                     ROOT / 'core/A2FOExtensions.def',
                     SLICE / 'exports-contract.def'):
            pins.add(path)
        for directory in ('tests', 'sdk'):
            for path in (ROOT / directory).rglob('*'):
                if path.is_file():
                    pins.add(path)
        record['pre_scan_inputs'] = dict(pins.digests)
        pins.verify()
        save(work, record)
        core.BEFORE = BEFORE
        core.SLICE = SLICE
        core.EXTRA_CORE_UNITS = ('core/module_loader.cpp',)
        core.EXTRA_CORE_TRAPS = record['extra_core_traps']
        record['frozen_hashes'] = frozen
        for name in SUITES:
            for version, archive in (('before', ORIGINAL), ('current', CANDIDATE)):
                exe = core.compile_case(name, version, archive, work, frozen,
                                        pins, record['dependencies'])
                record['cases'][f'{name}-{version}'] = {'exe': str(exe),
                                                        'sha256': sha(exe)}
                save(work, record)
        compile_single('ownership', [ROOT / 'tests/rust_runtime_ownership_native.cpp',
                                     ROOT / 'core/module_loader.cpp'],
                       CANDIDATE, work, pins, record)
        compile_single('handlers', [ROOT / 'tests/runtime_handlers_native.cpp'],
                       CANDIDATE, work, pins, record)
        compile_single('station', [ROOT / 'tests/runtime_station_state_native.cpp'],
                       CANDIDATE, work, pins, record)
        compile_single('dll-load', [ROOT / 'tests/rust_owned_dll_load.cpp'],
                       None, work, pins, record)
        normal_source = ROOT / 'modules/A2FONormalWeaponTech/module.cpp'
        pins.add(normal_source)
        compile_single('normal', [ROOT / 'tests/runtime_normal_ownership_native.cpp'],
                       CANDIDATE, work, pins, record,
                       extra_flags=(f'-DNORMAL_SOURCE="{normal_source}"',))
        compile_probe(work, pins, record)
        rva, address, base = private_rva(work, pins)
        record['private_globals'] = {'rva': rva, 'symbol_va': address,
                                     'image_base': base}
        record['inputs'] = dict(pins.digests)
        for path_string, digest in sorted(pins.digests.items()):
            path = Path(path_string)
            target = (work / 'source-used' / path.relative_to(ROOT) if
                      path.is_relative_to(ROOT) else
                      work / 'source-used/external' / digest / path.name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if sha(target) != digest:
                raise RuntimeError(f'source-used copy mismatch: {path}')
        record.pop('frozen_hashes')
        record['status'] = 'built'
        save(work, record)
        if args.build_only:
            return
        prefix = args.wine_prefix.resolve(strict=True)
        env = os.environ.copy()
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                   'winewayland.drv=d;winex11.drv=d', LIBGL_ALWAYS_SOFTWARE='1')
        for name in SUITES:
            traces = {}
            for version in ('before', 'current'):
                pins.verify()
                case = record['cases'][f'{name}-{version}']
                exe = Path(case['exe'])
                if sha(exe) != case['sha256']:
                    raise RuntimeError(f'executable drift: {exe}')
                runtime = work / 'runtime' / name / 'case'
                runtime.mkdir(parents=True, exist_ok=False)
                key = f'{name}-{version}-run'
                result = execute(['wine', str(exe)], work, key, pins,
                                 cwd=runtime, env=env, timeout=240)
                assets = capture_runtime(runtime, work, f'{name}/{version}')
                stdout = Path(result['stdout']).read_bytes().replace(b'\r\n', b'\n')
                stderr = Path(result['stderr']).read_bytes().replace(b'\r\n', b'\n')
                if any(marker not in stdout for marker in MARKERS[name]):
                    raise RuntimeError(f'{key}: historical completion marker absent')
                if name == 'dispatch' and (stdout.count(b'trigger-result:') != 10 or
                                           stdout.count(b'craft-result:') != 3):
                    raise RuntimeError(f'{key}: dispatch case count incomplete')
                traces[version] = (result['exit'], stdout, stderr, assets)
                record['runs'][key] = {'process': result, 'assets': assets,
                                       'stdout_sha256': hashlib.sha256(stdout).hexdigest(),
                                       'stderr_sha256': hashlib.sha256(stderr).hexdigest()}
                record['executions'] += 1
                save(work, record)
            if name == 'dispatch':
                before_exit, before_out, before_err, before_assets = traces['before']
                current_exit, current_out, current_err, current_assets = traces['current']
                before_parts = dispatch_sections(before_out)
                current_parts = dispatch_sections(current_out)
                record['dispatch_allocation_section'] = {
                    'before_sha256': hashlib.sha256(before_parts[1]).hexdigest(),
                    'current_sha256': hashlib.sha256(current_parts[1]).hexdigest(),
                    'equal': before_parts[1] == current_parts[1],
                    'before_raw': str(work / 'dispatch-before-run.stdout'),
                    'current_raw': str(work / 'dispatch-current-run.stdout')}
                if (before_exit, before_parts[0], before_parts[2], before_err,
                        before_assets) != (current_exit, current_parts[0],
                                           current_parts[2], current_err,
                                           current_assets):
                    raise RuntimeError('dispatch non-allocation frozen/current trace differs')
            elif traces['before'] != traces['current']:
                raise RuntimeError(f'{name}: frozen/current core trace differs')
            record['core_pairs'] += 1
            save(work, record)
        for name in ('ownership', 'handlers', 'station', 'dll-load', 'normal'):
            pins.verify()
            case = record['cases'][name]
            if sha(Path(case['exe'])) != case['sha256']:
                raise RuntimeError(f'{name}: executable drift')
            runtime = work / 'runtime' / name / 'case'
            runtime.mkdir(parents=True, exist_ok=False)
            executable = Path(case['exe'])
            argv = ['wine', str(executable)]
            if name == 'ownership':
                probe = record['cases']['module-probe']
                probe_dll = Path(probe['dll'])
                if sha(probe_dll) != probe['sha256']:
                    raise RuntimeError('module probe DLL drift')
                local_exe = runtime / 'ownership.exe'
                local_probe = runtime / probe_dll.name
                shutil.copy2(executable, local_exe)
                shutil.copy2(probe_dll, local_probe)
                if sha(local_exe) != case['sha256'] or sha(local_probe) != probe['sha256']:
                    raise RuntimeError('staged ownership/probe inputs differ')
                argv = ['wine', str(local_exe), local_probe.name]
            if name == 'dll-load':
                local_exe = runtime / 'rust_owned_dll_load.exe'
                local_dll = runtime / 'A2FOExtensions.dll'
                shutil.copy2(executable, local_exe)
                shutil.copy2(DLL, local_dll)
                if sha(local_exe) != case['sha256'] or sha(local_dll) != sha(DLL):
                    raise RuntimeError('staged Rust DLL load inputs differ')
                argv = ['wine', str(local_exe), 'A2FOExtensions.dll',
                        format(rva, 'x')]
            key = name + '-run'
            result = execute(argv, work, key, pins, cwd=runtime,
                             env=env, timeout=240)
            assets = capture_runtime(runtime, work, name)
            stdout = Path(result['stdout']).read_bytes().replace(b'\r\n', b'\n')
            stderr = Path(result['stderr']).read_bytes().replace(b'\r\n', b'\n')
            marker = {'ownership': b'rust-runtime-ownership-native:PASS',
                      'handlers': b'',
                      'station': b'runtime station state: passed',
                      'dll-load': b'rust-owned-dll:private-globals-and-retry:PASS',
                      'normal': b'normal ownership native: passed'}[name]
            if marker not in stdout:
                raise RuntimeError(f'{name}: completion marker absent')
            if name == 'ownership':
                for marker in (b'records:clone-move-byte-path:PASS',
                               b'records:explicit-reverse-shutdown:PASS',
                               b'records:clear-throw-diagnostic:PASS',
                               b'records:probe-reentrant-cleanup:PASS',
                               b'records:probe-throwing-logger:PASS'):
                    if stdout.count(marker) != 1:
                        raise RuntimeError('ownership case marker absent or repeated')
            if name == 'dll-load':
                log = (work / 'runtime-captured/dll-load/A2FOExtensions.log').read_bytes()
                for marker in (b'A2FOExtensions initialization started',
                               b'A2FOExtensions deferred initialization started',
                               b'ArmadaL.exe version mismatch',
                               b'No hooks installed'):
                    if marker not in log:
                        raise RuntimeError('DLL startup log marker absent')
            record['runs'][key] = {'process': result, 'assets': assets,
                                   'stdout_sha256': hashlib.sha256(stdout).hexdigest(),
                                   'stderr_sha256': hashlib.sha256(stderr).hexdigest()}
            record['executions'] += 1
            save(work, record)
        if (record['core_pairs'] != len(SUITES) or
                record['executions'] != record['expected_executions'] or
                len(record['cases']) != record['expected_compiled_cases']):
            raise RuntimeError('incomplete bounded ownership matrix')
        pins.verify()
        record['status'] = 'pass-bounded'
        save(work, record)
    except Exception as error:
        record['status'] = 'failed'
        record['error'] = str(error)
        record.pop('frozen_hashes', None)
        save(work, record)
        raise


if __name__ == '__main__':
    main()
