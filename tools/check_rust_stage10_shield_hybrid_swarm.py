#!/usr/bin/env python3
"""Pinned whole-source shield, Hybrid, and Swarm stage-ten native parity.

Only writes a fresh fixtures* child of shield-hybrid-swarm-batch. Wine execution
is serial and requires the lead's initialized private prefix.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/shield-hybrid-swarm-batch'
BEFORE = SLICE / 'before'
ORIGINAL = (ROOT / 'build/rust-migration-stage10/presentation-runtime-batch/'
            'checkpoint/artifacts/rust/i686-pc-windows-gnu/release/liba2fo_core.a')
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
DEFAULT_RECEIPT = SLICE / 'build-receipt-v2.json'
CXX = 'i686-w64-mingw32-g++'
SHIELD = 'modules/A2FOAlwaysShowShields/'
SWARM = 'modules/A2FOSwarmSystem/'
HYBRID = 'modules/A2FOHybridBuild/'
SHIELD_MODES = ('signature', 'first', 'second', 'third', 'root-throw',
                'success', 'gateway', 'render-list', 'render-activation')
SHIELD_SEEDS = {
    'root0/odf/parent.odf': b'alwaysShowShields = no\n',
    'root0/odf/child.odf': b'#include "parent.odf"\n',
    'root1/odf/parent.odf': b'alwaysShowShields = yes\n',
    'root1/odf/unused.odf': b'alwaysShowShields = 0\n',
    'root1/odf/CASE.odf': b'alwaysShowShields = yes\n',
    'root1/odf/case.odf': b'alwaysShowShields = no\n',
    'root1/odf/cyclea.odf': b'#include "cycleb.odf"\n',
    'root1/odf/cycleb.odf': b'#include "cyclea.odf"\n',
}
SWARM_MODES = ('init-invalid', 'init-signature', 'init-core-jump', 'init-partial',
               'init-success', 'parse-basic', 'parse-empty', 'parse-zero',
               'parse-no-hierarchy', 'class-throw', 'host-create',
               'host-model-fail', 'host-no-bounds', 'simulate-pc24',
               'simulate-pc53', 'simulate-pc64', 'simulate-phases',
               'simulate-expire', 'render-accepted', 'render-frustum',
               'render-invisible', 'hook-gates', 'hook-gateway-toggle')
HYBRID_SUITES = {
    'hybrid-provider': ('rust_stage8_hybrid_runtime_native.cpp',
                        ('normal', 'module', 'proc', 'provider', 'log')),
    'hybrid-state': ('rust_stage8_hybrid_state_native.cpp', ('normal',)),
    'hybrid-lists': ('rust_stage8_hybrid_lists_native.cpp', ('normal',)),
    'hybrid-stage9': ('rust_stage9_hybrid_runtime_native.cpp',
                      ('normal', 'log-failure')),
    'hybrid-completion': ('rust_stage10_hybrid_completion_native.cpp',
                          ('normal',)),
}
HYBRID_LIBS = ('-ladvapi32', '-lgdi32', '-lcomctl32', '-lversion',
               '-lws2_32', '-lshell32', '-lwinmm', '-lole32', '-lmsimg32',
               '-lgdiplus', '-ldxguid')
HYBRID_COMPLETION_ROWS = (
    b'queue-preferred:', b'queue-last:', b'queue-absent:',
    b'queue-invalid:', b'queue-cycle:', b'placement-first:',
    b'placement-absent:', b'bind-later-exact:', b'bind-first-empty:',
    b'clear-placements:', b'submenu-first:', b'ghosts:', b'roots:',
    b'rebase-live-mode:1', b'rebase-cached-button:1',
    b'roots-cached-buttons:1', b'submenu-register:',
    b'submenu-overflow:', b'publish:', b'remove-dedup:',
    b'hybrid-stage10-fixture:PASS',
)
HYBRID_FLAGS = ('-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                '-Werror', '-Wno-error=cast-function-type',
                '-Wno-unused-function', '-Wno-unused-variable',
                '-Wno-unused-parameter',
                '-DNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections',
                '-fdata-sections', '-static')
SUITES = tuple(f'shields-{flavor}' for flavor in ('standalone', 'builtin')) + \
         tuple(f'swarm-{flavor}' for flavor in ('standalone', 'builtin')) + \
         tuple(f'{name}-{flavor}-helpers' for name in ('shields', 'swarm')
               for flavor in ('standalone', 'builtin')) + \
         tuple(HYBRID_SUITES)
EXPECTED_PAIRS = 2 * len(SHIELD_MODES) + 2 * len(SWARM_MODES) + \
                 4 + \
                 sum(len(modes) for _, modes in HYBRID_SUITES.values())


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


class Pins:
    def __init__(self):
        self.files = {}

    def add(self, path, expected=None):
        path = Path(path).resolve(strict=True)
        actual = sha(path)
        if expected is not None and actual != expected:
            raise RuntimeError(f'authoritative input pin differs: {path}')
        if path in self.files and self.files[path] != actual:
            raise RuntimeError(f'input changed after pin: {path}')
        self.files[path] = actual

    def verify(self):
        drift = [str(path) for path, value in self.files.items()
                 if sha(path) != value]
        if drift:
            raise RuntimeError('input drift: ' + ', '.join(drift[:8]))


def save(work, state):
    (work / 'receipt.json').write_text(json.dumps(state, indent=2,
                                                   sort_keys=True) + '\n')


def execute(argv, work, key, pins, cwd=ROOT, env=None, timeout=360,
            expected=0):
    pins.verify()
    out = work / f'{key}.stdout'
    err = work / f'{key}.stderr'
    with out.open('wb') as stdout, err.open('wb') as stderr:
        try:
            result = subprocess.run(argv, cwd=cwd, env=env, stdout=stdout,
                                    stderr=stderr, timeout=timeout, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    record = {'argv': list(map(str, argv)), 'cwd': str(cwd), 'exit': code,
              'stdout': str(out), 'stdout_sha256': sha(out),
              'stderr': str(err), 'stderr_sha256': sha(err)}
    (work / f'{key}.json').write_text(json.dumps(record, indent=2) + '\n')
    pins.verify()
    if code != expected:
        raise RuntimeError(f'{key}: exit {code}, expected {expected}; see {err}')
    return record


def at(version, logical):
    return (BEFORE if version == 'before' else ROOT) / logical


def dependency_scan(argv, unit, work, key, pins, frozen, state):
    # argv is the exact compile invocation, with the unit immediately before -c.
    flags = argv[1:argv.index(str(unit))]
    scan = [CXX, *flags, '-MM']
    if unit.suffix == '.S':
        scan += ['-x', 'assembler-with-cpp']
    scan.append(str(unit))
    record = execute(scan, work, key, pins, timeout=180)
    output = Path(record['stdout']).read_text().replace('\\\n', ' ')
    if ':' not in output:
        raise RuntimeError(f'{key}: no dependency output')
    closure = sorted({Path(word).resolve() for word in
                      shlex.split(output.split(':', 1)[1])})
    if not closure:
        raise RuntimeError(f'{key}: empty dependency closure')
    for path in closure:
        if path.is_relative_to(BEFORE):
            logical = path.relative_to(BEFORE).as_posix()
            if logical not in frozen:
                raise RuntimeError(f'unpinned frozen dependency: {logical}')
            pins.add(path, frozen[logical])
        elif '-before-' in key and path.is_relative_to(ROOT) and not path.is_relative_to(work):
            raise RuntimeError(f'frozen dependency escaped snapshot: {path}')
        else:
            pins.add(path)
    state['dependency_scans'][key] = [str(path) for path in closure]


def hybrid_source(version, work, pins):
    original = at(version, HYBRID + 'hybrid_production_runtime.cpp')
    content = original.read_text()
    if content.count('a2fo::startup_module_provider(') != 1:
        raise RuntimeError(f'Hybrid provider transform marker changed: {original}')
    content = content.replace('__declspec(dllexport)', '')
    content = content.replace('a2fo::startup_module_provider(',
                              '::fixture_startup_provider(')
    content = re.sub(r'#include "([^"\n]+)"',
                     lambda match: '#include "' +
                     str((original.parent / match[1]).resolve()) + '"',
                     content)
    generated = work / f'hybrid-{version}-source.cpp'
    generated.write_text(content)
    pins.add(generated)
    return generated


def helper_fixture(version, family, work, pins):
    shared = ROOT / 'tests/rust_stage10_visibility_swarm_native.cpp'
    content = shared.read_text()
    for historical in ('rust_stage8_a_shields_native.cpp',
                       'rust_stage8_a_swarm_native.cpp'):
        marker = '"' + historical + '"'
        if content.count(marker) != 1:
            raise RuntimeError(f'shared helper fixture marker changed: {marker}')
        content = content.replace(marker,
                                  '"' + str(at(version, 'tests/' + historical)) + '"')
    generated = work / f'{family}-{version}-helpers.cpp'
    generated.write_text(content)
    pins.add(generated)
    return generated


def hybrid_completion_fixture(version, work, pins):
    shared = ROOT / 'tests/rust_stage10_hybrid_completion_native.cpp'
    content = shared.read_text()
    marker = '"rust_stage8_hybrid_runtime_native.cpp"'
    if content.count(marker) != 1:
        raise RuntimeError('shared Hybrid completion fixture marker changed')
    generated = work / f'hybrid-completion-{version}-fixture.cpp'
    generated.write_text(content.replace(marker, '"' + str(at(
        version, 'tests/rust_stage8_hybrid_runtime_native.cpp')) + '"'))
    pins.add(generated)
    return generated


def compile_case(suite, version, archive, work, pins, frozen, state,
                 generated, helpers, completion):
    key = f'{suite}-{version}'
    exe = work / f'{key}.exe'
    if suite.startswith('shields-'):
        flavor = suite.split('-')[1]
        unit = at(version, SHIELD + 'module.cpp')
        bridge = at(version, SHIELD + 'thiscall_bridge.S')
        fixture = (helpers[('shields', version)] if suite.endswith('-helpers')
                   else at(version, 'tests/rust_stage8_a_shields_native.cpp'))
        flags = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                 '-Werror', '-UNDEBUG', '-ffunction-sections',
                 '-fdata-sections', '-static',
                 f'-DMODULE_SOURCE="{unit}"']
        if flavor == 'builtin':
            flags += ['-DTEST_BUILTIN',
                      '-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::always_show_shields']
        if suite.endswith('-helpers'):
            flags += ['-DTEST_SHIELDS']
        units = [fixture, bridge]
        if version == 'before':
            units.append(at(version, SHIELD + 'shield_visibility.cpp'))
        libs = [str(archive), '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32']
    elif suite.startswith('swarm-'):
        flavor = suite.split('-')[1]
        unit = at(version, SWARM + 'module.cpp')
        fixture = (helpers[('swarm', version)] if suite.endswith('-helpers')
                   else at(version, 'tests/rust_stage8_a_swarm_native.cpp'))
        flags = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                 '-Werror', '-UNDEBUG', '-ffunction-sections',
                 '-fdata-sections', '-static',
                 f'-DMODULE_SOURCE="{unit}"']
        if flavor == 'builtin':
            flags += ['-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::swarm_system',
                      '-DA2FO_ModuleInit=A2FOBuiltinSwarmSystem_Init',
                      '-DA2FO_ModuleShutdown=A2FOBuiltinSwarmSystem_Shutdown']
        if suite.endswith('-helpers'):
            flags += ['-DTEST_SWARM']
        units = [fixture, at(version, SWARM + 'swarm_motion.cpp')]
        libs = [str(archive), '-ladvapi32']
    else:
        fixture = (completion[version] if suite == 'hybrid-completion'
                   else at(version, 'tests/' + HYBRID_SUITES[suite][0]))
        flags = list(HYBRID_FLAGS)
        if suite in ('hybrid-stage9', 'hybrid-completion'):
            flags.append('-Wno-error=mismatched-new-delete')
        flags.append(f'-DHYBRID_SOURCE="{generated}"')
        units = [fixture]
        if suite != 'hybrid-provider':
            units.append(at(version, HYBRID + 'hybrid_production.cpp'))
        if suite in ('hybrid-stage9', 'hybrid-completion'):
            units.append(at(version, HYBRID + 'build_submenu_config.cpp'))
        libs = [str(archive), *HYBRID_LIBS]
    for unit_path in units:
        pins.add(unit_path)
    if suite.startswith(('shields-', 'swarm-')):
        pins.add(unit)
    objects = []
    for index, source in enumerate(units):
        obj = work / f'{key}-{index}.o'
        command = [CXX, *flags, str(source), '-c', '-o', str(obj)]
        dependency_scan(command, source, work, f'{key}-dep-{index}',
                        pins, frozen, state)
        execute(command, work, f'{key}-compile-{index}', pins)
        objects.append(obj)
    if suite.startswith('hybrid-'):
        # Mirror the historical fixture: trap any uninvoked native engine path.
        undefined = execute(['i686-w64-mingw32-nm', '--undefined-only',
                             str(objects[0])], work, f'{key}-undefined', pins)
        provided = set()
        for index, obj in enumerate(objects[1:]):
            defined = execute(['i686-w64-mingw32-nm', '--defined-only',
                               str(obj)], work, f'{key}-defined-{index}', pins)
            provided.update(line.split()[-1] for line in
                            Path(defined['stdout']).read_text().splitlines()
                            if line.split())
        native = sorted({line.split()[-1] for line in
                         Path(undefined['stdout']).read_text().splitlines()
                         if line.split() and line.split()[-1].startswith(
                             ('_a2fo_', '__ZN4a2fo', '__ZNK4a2fo'))
                         and not line.split()[-1].startswith('_a2fo_rs_')
                         and line.split()[-1] not in provided})
        stubs = work / f'{key}-uninvoked.S'
        stubs.write_text('.text\n' + ''.join(
            '.globl ' + symbol + '\n' + symbol + ':\n'
            ' jmp _a2fo_hybrid_fixture_unexpected_native\n'
            for symbol in native))
        trap = work / f'{key}-trap.cpp'
        trap.write_text('#include <cstdio>\n#include <cstdlib>\n'
                        'extern "C" [[noreturn]] void '
                        'a2fo_hybrid_fixture_unexpected_native(){'
                        'std::fputs("unexpected native Hybrid engine path\\n",stderr);'
                        'std::abort();}\n')
        pins.add(stubs)
        pins.add(trap)
        objects += [stubs, trap]
    link = [CXX, '-static', '-Wl,--gc-sections', *map(str, objects),
            *libs, '-o', str(exe)]
    execute(link, work, f'{key}-link', pins)
    state['cases'][key] = {'exe': str(exe), 'exe_sha256': sha(exe),
                           'source': str(unit if suite.startswith(('shields-', 'swarm-'))
                                         else at(version, HYBRID + 'hybrid_production_runtime.cpp')),
                           'archive': str(archive), 'link': link}
    save(work, state)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--build-receipt', type=Path, default=DEFAULT_RECEIPT)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path('/tmp/a2fo-shields-wine-prefix'))
    args = parser.parse_args()
    work = args.work_dir.resolve()
    receipt_path = args.build_receipt.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('work directory must be a fresh fixtures* child of this batch')
    if receipt_path != DEFAULT_RECEIPT.resolve():
        parser.error('only the final batch build-receipt-v2.json is accepted')
    if not ORIGINAL.is_file() or not CANDIDATE.is_file() or not receipt_path.is_file():
        parser.error('original checkpoint archive, final candidate archive, and v2 build receipt required')
    if sha(ORIGINAL) == sha(CANDIDATE):
        parser.error('original and candidate archives are identical')
    build = json.loads(receipt_path.read_text())
    candidate_key = CANDIDATE.relative_to(ROOT).as_posix()
    if build.get('status') != 'passed' or build.get('artifacts', {}).get(candidate_key) != sha(CANDIDATE):
        parser.error('passed final build receipt does not pin candidate archive')
    current = build.get('inputs_after', {})
    if len(current) != 981 or not {'Makefile', 'rust/a2fo_core/Cargo.toml',
                           'rust/a2fo_core/Cargo.lock'}.issubset(current):
        parser.error('final v2 build receipt lacks 981 current product input pins')
    predecessor = json.loads((SLICE / 'predecessor-artifacts.json').read_text())
    original_key = ('build/rust-migration-stage10/presentation-runtime-batch/'
                    'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a')
    if predecessor.get(original_key) != sha(ORIGINAL):
        parser.error('original checkpoint archive differs from predecessor pin')
    frozen = json.loads((SLICE / 'before-hashes.json').read_text())
    if len(frozen) != 1251:
        parser.error('frozen baseline is not 1251 files')
    if not args.build_only and not (args.wine_prefix.resolve() / 'system.reg').is_file():
        parser.error('initialized exclusive private Wine prefix required')
    work.mkdir()
    state = {'status': 'preparing', 'build_only': args.build_only,
             'expected_pairs': EXPECTED_PAIRS, 'pairs': 0, 'executions': 0,
             'cases': {}, 'dependency_scans': {}, 'runs': {},
             'original_archive': str(ORIGINAL), 'original_sha256': sha(ORIGINAL),
             'candidate_archive': str(CANDIDATE), 'candidate_sha256': sha(CANDIDATE),
             'hybrid_mock_semantics': 'Historical -DNDEBUG; Hybrid fixtures contain no assert() calls',
             'shield_seed_sha256': {name: hashlib.sha256(contents).hexdigest()
                                    for name, contents in SHIELD_SEEDS.items()},
             'build_receipt': str(receipt_path),
             'build_receipt_sha256': sha(receipt_path)}
    save(work, state)
    try:
        pins = Pins()
        for logical, expected in frozen.items():
            pins.add(BEFORE / logical, expected)
        for logical, expected in current.items():
            pins.add(ROOT / logical, expected)
        for path in (ORIGINAL, CANDIDATE, receipt_path, Path(__file__),
                     SLICE / 'before-hashes.json', SLICE / 'predecessor-artifacts.json',
                     ROOT / 'ArmadaL.exe', ROOT / 'FleetOpsHook.dll'):
            pins.add(path)
        for directory in ('tests', 'sdk'):
            for path in (ROOT / directory).rglob('*'):
                if path.is_file():
                    pins.add(path)
        for name in ('check_rust_stage8_a_shields_parity.py',
                     'check_rust_stage8_a_swarm_parity.py',
                     'check_rust_stage8_hybrid_runtime_parity.py',
                     'check_rust_stage8_hybrid_state_parity.py',
                     'check_rust_stage8_hybrid_lists_parity.py',
                     'check_rust_stage9_hybrid_runtime_parity.py'):
            pins.add(ROOT / 'tools' / name)
        for version in ('before', 'current'):
            for historical, _ in HYBRID_SUITES.values():
                fixture = at(version, 'tests/' + historical)
                if historical.startswith('rust_stage10_'):
                    fixture = ROOT / 'tests' / historical
                if re.search(r'\bassert\s*\(', fixture.read_text()):
                    raise RuntimeError(f'Hybrid DNDEBUG mock contains assert: {fixture}')
        # Product inputs, fixtures, SDK, and game binaries are fixed before -MM.
        state['pre_scan_inputs'] = {str(path): value for path, value in pins.files.items()}
        pins.verify()
        save(work, state)
        generated = {version: hybrid_source(version, work, pins)
                     for version in ('before', 'current')}
        helpers = {(family, version): helper_fixture(version, family, work, pins)
                   for family in ('shields', 'swarm')
                   for version in ('before', 'current')}
        completion = {version: hybrid_completion_fixture(version, work, pins)
                      for version in ('before', 'current')}
        state['generated_sources'] = {version: {'path': str(path), 'sha256': sha(path)}
                                      for version, path in generated.items()}
        state['generated_helpers'] = {f'{family}-{version}':
                                      {'path': str(path), 'sha256': sha(path)}
                                      for (family, version), path in helpers.items()}
        state['generated_completion'] = {version: {'path': str(path), 'sha256': sha(path)}
                                         for version, path in completion.items()}
        pins.verify()
        save(work, state)
        for suite in SUITES:
            for version, archive in (('before', ORIGINAL), ('current', CANDIDATE)):
                compile_case(suite, version, archive, work, pins, frozen,
                             state, generated[version], helpers, completion)
        pins.verify()
        state['inputs'] = {str(path): value for path, value in pins.files.items()}
        for path, value in sorted(pins.files.items()):
            target = (work / 'source-used' / path.relative_to(ROOT) if
                      path.is_relative_to(ROOT) else
                      work / 'source-used/external' / value / path.name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if sha(target) != value:
                raise RuntimeError(f'source-used copy differs: {path}')
        state['status'] = 'built'
        save(work, state)
        if args.build_only:
            return
        prefix = args.wine_prefix.resolve(strict=True)
        env = os.environ.copy()
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                   'winewayland.drv=d;winex11.drv=d', LIBGL_ALWAYS_SOFTWARE='1')
        for suite in SUITES:
            modes = (('helpers',) if suite.endswith('-helpers') else
                     SHIELD_MODES if suite.startswith('shields-') else
                     SWARM_MODES if suite.startswith('swarm-') else
                     HYBRID_SUITES[suite][1])
            for mode in modes:
                pair = f'{suite}-{mode}'
                traces = {}
                for version in ('before', 'current'):
                    pins.verify()
                    case = state['cases'][f'{suite}-{version}']
                    exe = Path(case['exe'])
                    if sha(exe) != case['exe_sha256']:
                        raise RuntimeError(f'executable drift: {exe}')
                    # Both sides execute from the same logical working directory.
                    runtime = work / 'runtime' / pair / 'case'
                    runtime.mkdir(parents=True, exist_ok=False)
                    run_env = env.copy()
                    if suite.startswith('shields-'):
                        # Fixed logical C: path for each side; capture its assets after each run.
                        case_root = prefix / 'drive_c' / 'a2fo-stage10-shield-fixture'
                        if case_root.exists():
                            raise RuntimeError(f'shield fixture root already exists: {case_root}')
                        case_root.mkdir()
                        for relative, contents in SHIELD_SEEDS.items():
                            target = case_root / relative
                            target.parent.mkdir(parents=True, exist_ok=True)
                            target.write_bytes(contents)
                        run_env['A2FO_SHIELDS_ROOT'] = 'C:\\a2fo-stage10-shield-fixture'
                    key = f'{pair}-{version}-run'
                    expected = (77 if suite == 'hybrid-provider' and mode != 'normal'
                                or suite == 'hybrid-stage9' and mode != 'normal' else 0)
                    argv = ['wine', str(exe)]
                    if not suite.endswith('-helpers') and \
                            (suite.startswith(('shields-', 'swarm-')) or mode != 'normal'):
                        argv.append(mode)
                    record = execute(argv, work, key, pins, cwd=runtime,
                                     env=run_env, timeout=240, expected=expected)
                    captured = work / 'runtime-captured' / pair / version
                    captured.parent.mkdir(parents=True, exist_ok=True)
                    runtime.rename(captured)
                    assets = {path.relative_to(captured).as_posix(): sha(path)
                              for path in captured.rglob('*') if path.is_file()}
                    if suite.startswith('shields-'):
                        shield_captured = work / 'shield-root-captured' / pair / version
                        shield_captured.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copytree(case_root, shield_captured)
                        original_assets = {
                            path.relative_to(case_root).as_posix(): sha(path)
                            for path in case_root.rglob('*') if path.is_file()}
                        copied_assets = {
                            path.relative_to(shield_captured).as_posix(): sha(path)
                            for path in shield_captured.rglob('*') if path.is_file()}
                        if copied_assets != original_assets:
                            raise RuntimeError(f'{key}: shield root capture differs')
                        shutil.rmtree(case_root)
                        assets.update({'shield-root/' + relative: digest
                                       for relative, digest in copied_assets.items()})
                    stdout = Path(record['stdout']).read_bytes().replace(b'\r\n', b'\n')
                    stderr = Path(record['stderr']).read_bytes().replace(b'\r\n', b'\n')
                    if not stdout:
                        raise RuntimeError(f'{key}: native fixture produced no trace')
                    marker = {'hybrid-provider': b'hybrid provider fixture: PASS',
                              'hybrid-state': b'hybrid state fixture: PASS',
                              'hybrid-lists': b'hybrid lists fixture: PASS',
                              'hybrid-stage9': b'hybrid-stage9-fixture:PASS',
                              'hybrid-completion': b'hybrid-stage10-fixture:PASS'}.get(suite)
                    if suite.endswith('-helpers'):
                        marker = (b'visibility native target functions PASS'
                                  if suite.startswith('shields-') else
                                  b'swarm native target function PASS')
                    if marker and mode in ('normal', 'helpers') and marker not in stdout:
                        raise RuntimeError(f'{key}: expected completion marker absent')
                    if suite == 'hybrid-completion':
                        lines = stdout.splitlines()
                        missing = [row for row in HYBRID_COMPLETION_ROWS
                                   if sum(line.startswith(row) for line in lines) != 1]
                        if missing:
                            raise RuntimeError(f'{key}: missing or repeated Hybrid rows: {missing}')
                    if suite.startswith('hybrid-') and mode != 'normal' and b'terminate:' not in stdout:
                        raise RuntimeError(f'{key}: failure path not exercised')
                    state['runs'][key] = {'process': record, 'assets': assets,
                                          'normalized_stdout_sha256': hashlib.sha256(stdout).hexdigest(),
                                          'normalized_stderr_sha256': hashlib.sha256(stderr).hexdigest()}
                    traces[version] = (record['exit'], stdout, stderr, assets)
                    state['executions'] += 1
                    save(work, state)
                if traces['before'] != traces['current']:
                    raise RuntimeError(f'frozen/current native trace mismatch: {pair}')
                state['pairs'] += 1
                save(work, state)
        if state['pairs'] != EXPECTED_PAIRS or state['executions'] != 2 * EXPECTED_PAIRS:
            raise RuntimeError('incomplete native matrix')
        pins.verify()
        state['status'] = 'pass'
        save(work, state)
    except Exception as error:
        state['status'] = 'failed'
        state['error'] = str(error)
        save(work, state)
        raise


if __name__ == '__main__':
    main()
