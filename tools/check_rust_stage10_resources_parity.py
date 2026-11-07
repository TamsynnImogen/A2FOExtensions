#!/usr/bin/env python3
"""Pin and compare full frozen/current A2FOResources Windows fixture traces."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/resources'
STAGE8 = ROOT / 'build/rust-migration-stage8'
UNIT = Path('modules/A2FOResources/module.cpp')
FROZEN = STAGE8 / 'before' / UNIT
ORIGINAL = ROOT / 'build/rust-migration-stage10/energy-systems/checkpoint/liba2fo_core-original-win.a'
ORIGINAL_SHA = '01258fbad72411f37bebabffc2efb3e70aaeb8d7b28cf2a6f32a0558f2357c36'
TESTS = {'workflow': ROOT / 'tests/rust_stage10_resources_native.cpp',
         'inherited': ROOT / 'tests/rust_stage10_resources_native.cpp',
         'presentation': ROOT / 'tests/rust_stage10_resources_native.cpp'}
INHERITED = ROOT / 'tests/rust_stage8_c_resources_mock.cpp'
PRESENTATION = ROOT / 'tests/rust_stage10_resource_presentation_mock.cpp'
FEATURE = ROOT / 'tests/rust_stage8_c_resources_featurepack_mock.cpp'
ADAPTER = ROOT / 'modules/A2FOResources/additional_resources.cpp'
FLAVORS = ('standalone', 'builtin')


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def execute(argv, stdout, stderr, env=None):
    try:
        with stdout.open('wb') as out, stderr.open('wb') as err:
            return subprocess.run(argv, cwd=ROOT, env=env, stdout=out,
                                  stderr=err, timeout=300, check=False).returncode
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f'timeout: {argv[0]}; see {stderr}') from error


def dependencies(test, source, symbol, work, label, commands):
    argv = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
            f'-D{symbol}="{source}"', str(test)]
    if label.startswith('inherited-'):
        argv.insert(-1, '-DA2FO_RESOURCES_INHERITED=1')
    elif label.startswith('presentation-'):
        argv.insert(-1, '-DA2FO_RESOURCES_PRESENTATION=1')
        argv.insert(-1, f'-DA2FO_RESOURCES_PRESENTATION_FIXTURE="{work / "presentation-explicit-return.cpp"}"')
    stdout = work / f'deps-{label}.stdout.log'
    stderr = work / f'deps-{label}.stderr.log'
    code = execute(argv, stdout, stderr)
    commands.append({'kind': 'dependencies', 'label': label, 'argv': argv,
                     'exit_code': code, 'stdout_sha256': sha(stdout),
                     'stderr_sha256': sha(stderr)})
    if code:
        (work / 'receipt.json').write_text(json.dumps(
            {'status': 'failed', 'error': f'dependencies {label} exit {code}',
             'commands': commands}, indent=2) + '\n')
        raise RuntimeError(f'dependencies {label} exit {code}; see {stderr}')
    entries = stdout.read_text().replace('\\\n', ' ').split(':', 1)[1].split()
    return sorted({str(Path(entry).resolve().relative_to(ROOT)) for entry in entries
                   if Path(entry).resolve().is_relative_to(ROOT)})


def validate(kind, output):
    if kind == 'workflow':
        for marker in (b'ready:resources-workflow\n',
                       b'completed:resources-workflow\n',
                       b'check:can-pay-cost-once\n',
                       b'check:payment-cost-once\n',
                       b'check:debit-reentry-value\n',
                       b'check:class-cost-fallback\n',
                       b'check:fallback-can-pay\n',
                       b'check:zero-cost-skips-team-read\n',
                       b'check:malformed-kind-skips-costs\n',
                       b'check:commit-lock-order\n',
                       b'check:final-lock-recheck-blocked-debit\n',
                       b'check:infinite-skips-costs\n',
                       b'check:a1-legacy-route\n',
                       b'check:nonlegacy-gateway-order\n',
                       b'check:nonlegacy-cache-and-config\n',
                       b'check:nonlegacy-amount-lines\n',
                       b'check:nonlegacy-cache-fresh-reuse\n',
                       b'check:gateway-disable-order\n',
                       b'check:gateway-disable-skips-cache\n',
                       b'check:credit-at-maximum\n',
                       b'initial:10:20:30:40\n',
                       b'after-payment:8:18:27:36\n',
                       b'after-refund:9:20:30:40\n',
                       b'infinite-unchanged:9:20:30:40\n',
                       b'inject:before-final-lock\n',
                       b'raced-payment:0:20:30:40\n',
                       b'panel-live:9223372036854775807:44:1\n',
                       b'saturation:9223372036854775807:22:33:44\n'):
            if output.count(marker) != 1:
                raise RuntimeError(f'workflow missing unique expected row: {marker!r}')
        if output.count(b'cost-read:') != 8:
            raise RuntimeError('workflow must record exactly eight cost callback reads')
        if sum(line.startswith(b'cw:') for line in output.splitlines()) != 12:
            raise RuntimeError('workflow must record all 12 x87 control projections')
        if sum(line.startswith(b'rectcw:') for line in output.splitlines()) != 24:
            raise RuntimeError('workflow must record 24 x87 rectangle projections')
    elif kind == 'inherited':
        for marker in (b'ready:resources-inherited\n',
                       b'completed:resources-inherited\n',
                       b'find:1:12\n', b'costs:12:0:99:42\n',
                       b'api-can-pay:1\n', b'api-after-pay:18\n',
                       b'api-after-refund:30\n', b'panel-context:1:10:20:110:50\n',
                       b'runtime-success:1\n', b'panel-hook:1\n',
                       b'presentation-hooks:1\n', b'draw-result:1\n',
                       b'init-success:1\n'):
            if output.count(marker) != 1:
                raise RuntimeError(f'inherited missing unique expected row: {marker!r}')
        if len(output.splitlines()) < 40:
            raise RuntimeError('inherited fixture trace incomplete')
    else:
        if output.count(b'ready:resources-presentation\n') != 1 or \
           output.count(b'completed:resources-presentation\n') != 1:
            raise RuntimeError('presentation missing unique ready/completed markers')
        if sum(line.startswith(b'PASS:') for line in output.splitlines()) != 37:
            raise RuntimeError('presentation expected exactly 37 assertion rows')
        for marker in (b'PASS:key-copy-allocation-failure\n',
                       b'PASS:assignment-allocation-failure\n',
                       b'PASS:translation-temporary-allocation-failure\n',
                       b'PASS:public-api\n', b'PASS:public-api-inactive\n'):
            if output.count(marker) != 1:
                raise RuntimeError(f'presentation missing assertion row: {marker!r}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--rust-lib', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('--work-dir must be a new fixtures* child of the resources slice')
    candidate = args.rust_lib.resolve(strict=True)
    pins = json.loads((STAGE8 / 'before-hashes.json').read_text())
    if sha(FROZEN) != pins[str(UNIT)] or sha(ORIGINAL) != ORIGINAL_SHA:
        raise RuntimeError('frozen source or independent original archive drift')
    if sha(ADAPTER) != pins[str(ADAPTER.relative_to(ROOT))]:
        raise RuntimeError('original additional resource adapter drift')
    work.mkdir(parents=True)
    presentation_original = PRESENTATION.read_bytes()
    presentation_tail = b'    VirtualFree(memory, 0, MEM_RELEASE);\n}\n'
    if presentation_original.count(b'int main() {') != 1 or \
       not presentation_original.endswith(presentation_tail):
        raise RuntimeError('presentation fixture main shape changed')
    presentation_generated = (presentation_original[:-2] +
                              b'    return 0;\n}\n')
    generated_presentation = work / 'presentation-explicit-return.cpp'
    generated_presentation.write_bytes(presentation_generated)
    if generated_presentation.read_bytes() != presentation_generated:
        raise RuntimeError('presentation fixture transformation write mismatch')
    transformation = {
        'source': str(PRESENTATION.relative_to(ROOT)),
        'source_sha256': sha(PRESENTATION),
        'generated': str(generated_presentation.relative_to(ROOT)),
        'generated_sha256': sha(generated_presentation),
        'change': 'insert explicit return 0 before final main closing brace',
    }
    preflight_commands = []
    paths = {'checker': Path(__file__).resolve(), 'frozen_source': FROZEN,
             'current_source': ROOT / UNIT, 'original_archive': ORIGINAL,
             'candidate_archive': candidate,
             'before_manifest': STAGE8 / 'before-hashes.json',
             'feature_fixture': FEATURE, 'inherited_fixture': INHERITED,
             'presentation_fixture': PRESENTATION,
             'presentation_explicit_return': generated_presentation,
             'original_adapter': ADAPTER,
             **{f'test_{k}': v for k, v in TESTS.items()}}
    deps = {}
    for kind, test in TESTS.items():
        symbol = ('MODULE_SOURCE' if kind == 'workflow' else
                  'A2FO_STAGE8_RESOURCES_SOURCE' if kind == 'inherited' else
                  'A2FO_STAGE10_RESOURCES_SOURCE')
        for label, source in (('before', FROZEN), ('current', ROOT / UNIT)):
            key = f'{kind}-{label}'
            deps[key] = dependencies(test, source, symbol, work, key,
                                     preflight_commands)
            (work / f'{key}-dependencies.txt').write_text('\n'.join(deps[key]) + '\n')
            for entry in deps[key]:
                paths.setdefault(f'dependency:{entry}', ROOT / entry)
    adapter_command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM', str(ADAPTER)]
    adapter_stdout = work / 'deps-adapter.stdout.log'
    adapter_stderr = work / 'deps-adapter.stderr.log'
    adapter_code = execute(adapter_command, adapter_stdout, adapter_stderr)
    preflight_commands.append({'kind': 'dependencies', 'label': 'adapter',
                               'argv': adapter_command, 'exit_code': adapter_code,
                               'stdout_sha256': sha(adapter_stdout),
                               'stderr_sha256': sha(adapter_stderr)})
    if adapter_code:
        (work / 'receipt.json').write_text(json.dumps(
            {'status': 'failed', 'error': f'adapter dependencies exit {adapter_code}',
             'commands': preflight_commands}, indent=2) + '\n')
        raise RuntimeError(f'adapter dependencies exit {adapter_code}')
    adapter_entries = adapter_stdout.read_text().replace('\\\n', ' ').split(':', 1)[1].split()
    deps['adapter'] = sorted({str(Path(entry).resolve().relative_to(ROOT))
                              for entry in adapter_entries
                              if Path(entry).resolve().is_relative_to(ROOT)})
    (work / 'adapter-dependencies.txt').write_text('\n'.join(deps['adapter']) + '\n')
    for entry in deps['adapter']:
        paths.setdefault(f'dependency:{entry}', ROOT / entry)
        if entry not in pins or sha(ROOT / entry) != pins[entry]:
            raise RuntimeError(f'unverified original adapter dependency: {entry}')
    for entry in set(deps['workflow-before'] + deps['inherited-before'] +
                     deps['presentation-before']):
        if entry in {str(FROZEN.relative_to(ROOT)), str(INHERITED.relative_to(ROOT)),
                     str(PRESENTATION.relative_to(ROOT)),
                     str(generated_presentation.relative_to(ROOT)),
                     *(str(path.relative_to(ROOT)) for path in TESTS.values())}:
            continue
        logical = entry.removeprefix('build/rust-migration-stage8/before/')
        if logical not in pins or sha(ROOT / entry) != pins[logical]:
            raise RuntimeError(f'unverified frozen dependency: {entry}')
    for entry in set(deps['workflow-current'] + deps['inherited-current'] +
                     deps['presentation-current']):
        if entry in {str(UNIT), str(INHERITED.relative_to(ROOT)),
                     str(PRESENTATION.relative_to(ROOT)),
                     str(generated_presentation.relative_to(ROOT)),
                     *(str(path.relative_to(ROOT)) for path in TESTS.values())}:
            continue
        if entry in pins and sha(ROOT / entry) != pins[entry]:
            raise RuntimeError(f'original header drift: {entry}')
    for path in sorted((ROOT / 'rust/a2fo_core/src').rglob('*.rs')):
        paths[f'rust_source:{path.relative_to(ROOT)}'] = path
    for path in sorted((ROOT / 'rust').glob('*resources*ffi.h')):
        paths[f'rust_header:{path.relative_to(ROOT)}'] = path
    before = {name: sha(path) for name, path in paths.items()}
    for name, path in paths.items():
        target = work / 'source-used' / name / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        if sha(target) != before[name]:
            raise RuntimeError(f'snapshot mismatch: {name}')
    receipt = {'status': 'building', 'inputs_before': before,
               'presentation_transformation': transformation,
               'dependencies': deps, 'commands': preflight_commands,
               'binaries': {}, 'traces': {},
               'flavors': FLAVORS, 'cases': tuple(TESTS),
               'assertions_enabled': True,
               'expected_trace_pairs': len(FLAVORS) * len(TESTS)}

    def save():
        receipt['inputs_after'] = {name: sha(path) for name, path in paths.items()}
        (work / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')

    def check_inputs():
        changed = [name for name, path in paths.items() if sha(path) != before[name]]
        if changed:
            raise RuntimeError('input drift: ' + ', '.join(changed))

    def command(argv, label, kind, flavor='', version='', env=None):
        stdout = work / f'{label}.stdout.log'
        stderr = work / f'{label}.stderr.log'
        code = execute(argv, stdout, stderr, env)
        receipt['commands'].append({'kind': kind, 'flavor': flavor,
                                    'version': version, 'argv': argv,
                                    'exit_code': code, 'stdout_sha256': sha(stdout),
                                    'stderr_sha256': sha(stderr)})
        check_inputs()
        save()
        if code:
            raise RuntimeError(f'{label} exit {code}; see {stderr}')
        return stdout.read_bytes().replace(b'\r\n', b'\n')

    try:
        feature = work / 'A2FOFeaturePack.dll'
        command(['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
                 '-Wpedantic', '-Werror', '-shared', '-static-libgcc', str(FEATURE),
                 '-o', str(feature)], 'build-feature', 'compile')
        receipt['binaries']['feature'] = sha(feature)
        for flavor in FLAVORS:
            for kind, test in TESTS.items():
                symbol = ('MODULE_SOURCE' if kind == 'workflow' else
                          'A2FO_STAGE8_RESOURCES_SOURCE' if kind == 'inherited' else
                          'A2FO_STAGE10_RESOURCES_SOURCE')
                for version, source, archive in (('before', FROZEN, ORIGINAL),
                                                 ('current', ROOT / UNIT, candidate)):
                    executable = work / f'{kind}-{flavor}-{version}.exe'
                    argv = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
                            '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
                            '-ffunction-sections', '-fdata-sections',
                            '-Wl,--gc-sections', '-static',
                            f'-D{symbol}="{source}"']
                    if flavor == 'builtin':
                        argv.append('-DA2FO_BUILTIN_STARTUP=1')
                    if kind == 'inherited':
                        argv.append('-DA2FO_RESOURCES_INHERITED=1')
                    elif kind == 'presentation':
                        argv.append('-DA2FO_RESOURCES_PRESENTATION=1')
                        argv.append(f'-DA2FO_RESOURCES_PRESENTATION_FIXTURE="{generated_presentation}"')
                        # Allocation hooks in this fixture require the same
                        # warning exception as its original checker.
                        argv.append('-Wno-mismatched-new-delete')
                    argv += [str(test)]
                    if kind == 'workflow':
                        argv.append(str(ADAPTER))
                    argv += [str(archive), '-o', str(executable)]
                    command(argv, f'build-{kind}-{flavor}-{version}', 'compile',
                            flavor, version)
                    receipt['binaries'][f'{kind}-{flavor}-{version}'] = sha(executable)
        if args.build_only:
            receipt['status'] = 'built'
            save()
            print(f'PASS built resource workflow and inherited fixtures: {work}')
            return 0
        prefix = (args.wine_prefix.resolve() if args.wine_prefix
                  else work / 'wine-prefix')
        env = {**os.environ, 'WINEPREFIX': str(prefix), 'WINEDEBUG': '-all',
               'DISPLAY': '', 'WAYLAND_DISPLAY': '',
               'WINEDLLOVERRIDES': 'winemenubuilder.exe=d;winedbg.exe=d;'
                                    'winewayland.drv=d;winex11.drv=d'}
        receipt['status'] = 'running'
        for flavor in FLAVORS:
            for kind in TESTS:
                traces = {}
                for version in ('before', 'current'):
                    executable = work / f'{kind}-{flavor}-{version}.exe'
                    trace = command(['wine', str(executable)],
                                    f'run-{kind}-{flavor}-{version}', 'run',
                                    flavor, version, env)
                    validate(kind, trace)
                    traces[version] = trace
                    receipt['traces'][f'{kind}-{flavor}-{version}'] = hashlib.sha256(trace).hexdigest()
                if traces['before'] != traces['current']:
                    a, b = (trace.splitlines() for trace in traces.values())
                    for index in range(max(len(a), len(b))):
                        if a[index:index + 1] != b[index:index + 1]:
                            raise RuntimeError(f'{kind}/{flavor} first difference line {index + 1}: '
                                               f'{a[index:index + 1]} != {b[index:index + 1]}')
        receipt['status'] = 'passed'
        save()
        print(f'PASS {receipt["expected_trace_pairs"]} resource trace pairs: {work}')
        return 0
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    sys.exit(main())
