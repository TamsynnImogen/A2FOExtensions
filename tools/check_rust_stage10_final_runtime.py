#!/usr/bin/env python3
"""Pinned final-runtime Breeders, Infestation, and M3D native parity.

Only writes a fresh fixtures* child of final-runtime-batch. Wine execution
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
SLICE = ROOT / 'build/rust-migration-stage10/final-runtime-batch'
BEFORE = SLICE / 'before'
ORIGINAL = (ROOT / 'build/rust-migration-stage10/shield-hybrid-swarm-batch/'
            'checkpoint/artifacts/rust/i686-pc-windows-gnu/release/liba2fo_core.a')
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
DEFAULT_RECEIPT = SLICE / 'build-receipt-v1.json'
CXX = 'i686-w64-mingw32-g++'
BREEDERS_MODES = ('metadata', 'metadata-null', 'metadata-invalid', 'nearest',
    'numeric24', 'numeric53', 'numeric64', 'request', 'no-mother', 'waiting',
    'active', 'disabled', 'short-producer', 'short-craft', 'cleanup', 'shutdown',
    'init-signature', 'init-alias-false', 'init-alias-throw', 'init-class-throw',
    'init-craft-false', 'init-success')
INFESTATION_MODES = ('metadata', 'metadata-null', 'native-badalloc', 'trigger',
    'hatch', 'cure', 'route', 'nearest', 'numeric24', 'numeric53', 'numeric64',
    'disabled', 'short', 'init-signature', 'init-class-false',
    'init-weapon-throw', 'init-success')
M3D_MODES = ('init-short', 'init-no-armada', 'init-armada-throw',
    'init-roots-throw', 'init-signature', 'init-hook-first', 'init-hook-second',
    'init-hook-throw', 'init-success', 'cached', 'cached-no-fallback',
    'cached-invalid', 'resolver-off', 'resolver-null', 'resolver-missing',
    'load-fail', 'hierarchy', 'names')
LIFECYCLE = {'breeders': ('success', 'signature', 'registration', 'dispatch'),
             'infestation': ('success', 'signature', 'registration', 'dispatch'),
             'm3d': ('success', 'signature', 'partial')}
SUITES = ('breeders', 'infestation', 'm3d-standalone', 'm3d-builtin',
          'breeders-lifecycle', 'infestation-lifecycle', 'm3d-lifecycle',
          'm3d-standalone-completion', 'm3d-builtin-completion',
          'm3d-conversion-host', 'm3d-hierarchy-win32')
MODES = {'breeders': BREEDERS_MODES, 'infestation': INFESTATION_MODES,
         'm3d-standalone': M3D_MODES, 'm3d-builtin': M3D_MODES,
         **{family + '-lifecycle': modes for family, modes in LIFECYCLE.items()},
         **{suite: ('normal',) for suite in SUITES[7:]}}
EXPECTED_PAIRS = sum(len(modes) for modes in MODES.values())
WIN_LIBS = ('-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32')
FLAGS = ('-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-UNDEBUG', '-ffunction-sections', '-fdata-sections', '-static')


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
    scan = [argv[0], *flags, '-MM']
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


def generated_fixture(suite, version, work, pins):
    if suite.endswith('-completion'):
        template = ROOT / 'tests/rust_stage10_m3d_completion_native.cpp'
        historical = at(version, 'tests/rust_stage8_a_m3d_native.cpp')
        content = template.read_text()
        marker = '#include STAGE8_M3D_FIXTURE'
        if content.count(marker) != 1:
            raise RuntimeError('M3D completion fixture include marker changed')
        content = content.replace(marker, '#include "' + str(historical) + '"')
    elif suite.endswith('-lifecycle'):
        family = suite.split('-')[0]
        template = at(version, 'tests/builtin_' + family + '_native_test.cpp')
        wrapper = at(version, 'core/builtin_' + family + '.cpp')
        module_registry = at(version, 'core/builtin_modules.cpp')
        content = template.read_text()
        marker = '#include "../core/builtin_' + family + '.cpp"'
        if content.count(marker) != 1:
            raise RuntimeError(f'{family} lifecycle wrapper marker changed')
        content = content.replace(marker, '#include "' + str(wrapper) + '"')
        content = re.sub(r'#include "([^"\n]+)"',
            lambda match: '#include "' + str(
                (template.parent / match[1]).resolve()) + '"'
            if not Path(match[1]).is_absolute() else match[0], content)
        declarations = re.findall(
            r'bool(?: A2FO_CALL)? (A2FOBuiltin\w+_(?:Init|Ready))\([^;]*\)(?: noexcept)?;',
            module_registry.read_text())
        if len(declarations) < 35 or len(declarations) != len(set(declarations)):
            raise RuntimeError('built-in registry stub declaration set changed')
        content += '\n#include <cstdlib>\nextern "C" {\n'
        for name in declarations:
            if name == 'A2FOBuiltin' + family.title() + '_Init' or \
                    name == 'A2FOBuiltin' + family.title() + '_Ready':
                continue
            if name.endswith('_Init'):
                content += f'bool A2FO_CALL {name}(const A2FO_ModuleApi*)' + \
                    '{std::fputs("unexpected built-in init\\n",stderr);std::abort();}\n'
            else:
                content += f'bool {name}() noexcept' + \
                    '{std::fputs("unexpected built-in ready\\n",stderr);std::abort();}\n'
        for name in ('CameraControls', 'ViewDistance'):
            content += f'void A2FOBuiltin{name}_SetEnabled(bool enabled) noexcept' + \
                '{if(enabled){std::fputs("unexpected enabled built-in gate\\n",stderr);' + \
                'std::abort();}std::puts("gate:' + name + ':0");}\n'
        content += '}\nnamespace a2fo { bool initialize_startup_builtin(' + \
            'BuiltinFeature, const A2FO_ModuleApi&)' + \
            '{std::fputs("unexpected startup init\\n",stderr);std::abort();} }\n'
        if family != 'm3d':
            content += 'extern "C" [[noreturn]] void a2fo_final_fixture_unexpected_native()' + \
                '{std::fputs("unexpected native engine bridge\\n",stderr);std::abort();}\n'
    else:
        return None
    output = work / f'{suite}-{version}-fixture.cpp'
    output.write_text(content)
    pins.add(output)
    return output


def unit_specs(suite, version, generated):
    flags = list(FLAGS)
    libs = list(WIN_LIBS)
    if suite in ('breeders', 'infestation'):
        family = suite
        module = at(version, 'modules/A2FO' + family.title() + '/module.cpp')
        fixture = at(version, 'tests/rust_stage8_a_' + family + '_native.cpp')
        flags.append(f'-DMODULE_SOURCE="{module}"')
        if family == 'infestation':
            flags.append('-Wno-error=mismatched-new-delete')
        return [fixture], flags, libs, module
    if suite.startswith('m3d-') and not suite.endswith('-lifecycle') and \
            suite not in ('m3d-hierarchy-win32', 'm3d-conversion-host'):
        module = at(version, 'modules/A2FOM3D/module.cpp')
        fixture = (generated if suite.endswith('-completion') else
                   at(version, 'tests/rust_stage8_a_m3d_native.cpp'))
        flags.append(f'-DMODULE_SOURCE="{module}"')
        if 'builtin' in suite:
            flags.append('-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::m3d')
        if suite.endswith('-completion'):
            flags.append('-Wno-error=mismatched-new-delete')
        units = [fixture, at(version, 'modules/A2FOM3D/m3d_conversion.cpp')]
        return units, flags, libs, module
    if suite.endswith('-lifecycle'):
        family = suite.split('-')[0]
        units = [generated, at(version, 'core/builtin_modules.cpp')]
        if family == 'm3d':
            units += [at(version, 'modules/A2FOM3D/m3d_conversion.cpp'),
                      at(version, 'modules/A2FOM3D/thiscall_bridge.S')]
        libs += ['-lgdi32', '-lcomctl32', '-lversion', '-lshell32',
                 '-lwinmm', '-lmsimg32', '-lgdiplus', '-ldxguid']
        return units, flags, libs, at(version, 'core/builtin_' + family + '.cpp')
    if suite == 'm3d-conversion-host':
        return [at(version, 'tests/m3d_conversion_test.cpp'),
                at(version, 'modules/A2FOM3D/m3d_conversion.cpp')], flags, [], None
    if suite == 'm3d-hierarchy-win32':
        return [at(version, 'tests/m3d_runtime_hierarchy_test.cpp'),
                at(version, 'modules/A2FOM3D/m3d_conversion.cpp'),
                at(version, 'modules/A2FOM3D/thiscall_bridge.S')], flags, libs, None
    raise RuntimeError(f'unknown suite: {suite}')


def compile_case(suite, version, archive, host_archive, work, pins,
                 frozen, state, generated):
    key = f'{suite}-{version}'
    host = suite == 'm3d-conversion-host'
    compiler = 'g++' if host else CXX
    units, flags, libs, source = unit_specs(suite, version, generated)
    if suite.endswith('-lifecycle') and suite != 'm3d-lifecycle':
        family = suite.split('-')[0]
        bridge = at(version, 'modules/A2FO' + family.title() + '/thiscall_bridge.S')
        pins.add(bridge)
        symbols = re.findall(r'^\s*\.globl\s+(_a2fo_' + family +
                             r'_\w+)\s*$',
                             bridge.read_text(), re.MULTILINE)
        if not symbols or len(symbols) != len(set(symbols)):
            raise RuntimeError(f'{family} native bridge symbol set changed')
        trap = work / f'{key}-native-traps.S'
        trap.write_text('.text\n' + ''.join(
            '.globl ' + symbol + '\n' + symbol + ':\n'
            ' jmp _a2fo_final_fixture_unexpected_native\n'
            for symbol in symbols))
        pins.add(trap)
        units.append(trap)
        state.setdefault('generated_native_traps', {})[key] = {
            'path': str(trap), 'sha256': sha(trap),
            'bridge_declarations': str(bridge), 'symbols': symbols}
    if host:
        flags = [f for f in flags if f != '-static']
    for unit in units:
        pins.add(unit)
    if source:
        pins.add(source)
    objects = []
    for index, unit in enumerate(units):
        obj = work / f'{key}-{index}.o'
        command = [compiler, *flags, str(unit), '-c', '-o', str(obj)]
        dependency_scan(command, unit, work, f'{key}-dep-{index}',
                        pins, frozen, state)
        execute(command, work, f'{key}-compile-{index}', pins)
        objects.append(obj)
    exe = work / (key + ('' if host else '.exe'))
    library = host_archive if host else archive
    link = [compiler, '-Wl,--gc-sections', *map(str, objects),
            str(library), *libs, '-o', str(exe)]
    if not host:
        link.insert(1, '-static')
    execute(link, work, f'{key}-link', pins)
    state['cases'][key] = {'exe': str(exe), 'exe_sha256': sha(exe),
                           'source': str(source) if source else None,
                           'archive': str(library), 'link': link}
    save(work, state)

def modes_for(suite):
    return MODES[suite]


def exit_and_marker(suite, mode):
    if suite.startswith('m3d-') and mode in ('init-armada-throw',
                                           'init-roots-throw', 'init-hook-throw'):
        return 1, {'init-armada-throw': b'fixture: armada injected',
                   'init-roots-throw': b'fixture: roots injected',
                   'init-hook-throw': b'fixture: hook injected'}[mode], 'stderr'
    markers = {'breeders-lifecycle': b'builtin breeders ' + mode.encode() + b': PASS',
               'infestation-lifecycle': b'builtin infestation ' + mode.encode() + b': PASS',
               'm3d-lifecycle': b'builtin M3D native fixture: PASS',
               'm3d-standalone-completion': b'm3d-stage10-completion:PASS',
               'm3d-builtin-completion': b'm3d-stage10-completion:PASS',
               'm3d-conversion-host': b'm3d conversion tests passed',
               'm3d-hierarchy-win32': b'm3d runtime hierarchy audit: passed'}
    if suite == 'infestation' and mode == 'native-badalloc':
        return 0, b'terminate:bad_alloc', 'stdout'
    return 0, markers.get(suite, b''), 'stdout'


def captured_files(path):
    return {item.relative_to(path).as_posix(): sha(item)
            for item in path.rglob('*') if item.is_file()}


def archive_temp(fixed_temp, work, pair, version):
    capture = work / 'temp-captured' / pair / version
    capture.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(fixed_temp, capture)
    observed = captured_files(fixed_temp)
    copied = captured_files(capture)
    if copied != observed:
        raise RuntimeError(f'{pair}-{version}: task temp capture differs')
    shutil.rmtree(fixed_temp)
    return {'temp/' + name: digest for name, digest in copied.items()}


def archive_optional_root(source, work, pair, version):
    capture = work / 'lifecycle-root-captured' / pair / version
    capture.parent.mkdir(parents=True, exist_ok=True)
    if source.exists():
        shutil.copytree(source, capture)
        observed = captured_files(source)
        copied = captured_files(capture)
        if copied != observed:
            raise RuntimeError(f'{pair}-{version}: lifecycle root capture differs')
        shutil.rmtree(source)
        return {'lifecycle-root/' + name: digest for name, digest in copied.items()}, True
    capture.mkdir()
    return {}, False


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
        parser.error('work directory must be a fresh fixtures* child of final batch')
    if receipt_path != DEFAULT_RECEIPT.resolve():
        parser.error('only the final batch build-receipt-v1.json is accepted')
    host_before = ORIGINAL.parents[2] / 'release/liba2fo_core.a'
    host_current = SLICE / 'candidate/rust/release/liba2fo_core.a'
    if not all(path.is_file() for path in
               (ORIGINAL, CANDIDATE, host_before, host_current, receipt_path)):
        parser.error('both Win32 and host archives and v1 receipt required')
    if sha(ORIGINAL) == sha(CANDIDATE):
        parser.error('original and candidate Win32 archives are identical')
    build = json.loads(receipt_path.read_text())
    artifact_keys = (CANDIDATE.relative_to(ROOT).as_posix(),
                     host_current.relative_to(ROOT).as_posix())
    if build.get('status') != 'passed' or any(
        build.get('artifacts', {}).get(key) != sha(ROOT / key)
        for key in artifact_keys):
        parser.error('passed final build receipt does not pin current archives')
    current = build.get('inputs_after', {})
    if len(current) != 983 or not {'Makefile', 'rust/a2fo_core/Cargo.toml',
                                   'rust/a2fo_core/Cargo.lock'}.issubset(current):
        parser.error('final v1 build receipt lacks 983 current product input pins')
    predecessor_path = SLICE / 'predecessor-artifacts.json'
    predecessor = json.loads(predecessor_path.read_text())
    for archive in (ORIGINAL, host_before):
        key = archive.relative_to(ROOT).as_posix().replace(
            '/checkpoint/artifacts/', '/candidate/')
        if predecessor.get(key) != sha(archive):
            parser.error(f'predecessor archive pin differs: {archive}')
    frozen_path = SLICE / 'before-hashes.json'
    frozen = json.loads(frozen_path.read_text())
    if len(frozen) != 1490:
        parser.error('frozen baseline is not 1490 files')
    if not args.build_only and not (args.wine_prefix.resolve() / 'system.reg').is_file():
        parser.error('initialized exclusive private Wine prefix required')
    work.mkdir()
    state = {'status': 'preparing', 'build_only': args.build_only,
             'expected_pairs': EXPECTED_PAIRS, 'pairs': 0, 'executions': 0,
             'cases': {}, 'dependency_scans': {}, 'runs': {},
             'before_archives': {str(path): sha(path)
                                 for path in (ORIGINAL, host_before)},
             'current_archives': {str(path): sha(path)
                                  for path in (CANDIDATE, host_current)},
             'build_receipt': str(receipt_path),
             'build_receipt_sha256': sha(receipt_path),
             'suite_modes': {suite: list(modes_for(suite)) for suite in SUITES}}
    save(work, state)
    try:
        pins = Pins()
        for logical, expected in frozen.items():
            pins.add(BEFORE / logical, expected)
        for logical, expected in current.items():
            pins.add(ROOT / logical, expected)
        for path in (ORIGINAL, CANDIDATE, host_before, host_current,
                     receipt_path, Path(__file__), frozen_path, predecessor_path,
                     ROOT / 'ArmadaL.exe', ROOT / 'FleetOpsHook.dll'):
            pins.add(path)
        for directory in ('tests', 'sdk'):
            for path in (ROOT / directory).rglob('*'):
                if path.is_file():
                    pins.add(path)
        for name in ('check_rust_stage8_a_breeders_parity.py',
                     'check_rust_stage8_a_infestation_parity.py',
                     'check_rust_stage8_a_m3d_parity.py'):
            pins.add(ROOT / 'tools' / name)
        state['pre_scan_inputs'] = {str(path): value for path, value in pins.files.items()}
        pins.verify()
        save(work, state)
        generated = {}
        for suite in SUITES:
            for version in ('before', 'current'):
                item = generated_fixture(suite, version, work, pins)
                if item:
                    generated[(suite, version)] = item
        state['generated_sources'] = {
            suite + '-' + version: {'path': str(path), 'sha256': sha(path)}
            for (suite, version), path in generated.items()}
        pins.verify()
        save(work, state)
        for suite in SUITES:
            for version, win_archive, host_archive in (
                    ('before', ORIGINAL, host_before),
                    ('current', CANDIDATE, host_current)):
                compile_case(suite, version, win_archive, host_archive,
                             work, pins, frozen, state,
                             generated.get((suite, version)))
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
            for mode in modes_for(suite):
                pair = f'{suite}-{mode}'
                traces = {}
                for version in ('before', 'current'):
                    pins.verify()
                    case = state['cases'][f'{suite}-{version}']
                    exe = Path(case['exe'])
                    if sha(exe) != case['exe_sha256']:
                        raise RuntimeError(f'executable drift: {exe}')
                    runtime = work / 'runtime' / pair / 'case'
                    runtime.mkdir(parents=True, exist_ok=False)
                    run_env = env.copy()
                    fixed_temp = prefix / 'drive_c/a2fo-stage10-final-runtime-temp'
                    lifecycle_root = (prefix / 'drive_c' /
                                      ('a2fo-' + suite.split('-')[0] + '-fixture')
                                      if suite in ('breeders-lifecycle',
                                                   'infestation-lifecycle') else None)
                    if lifecycle_root and lifecycle_root.exists():
                        raise RuntimeError(f'preexisting lifecycle root: {lifecycle_root}')
                    if suite.startswith('m3d-') and suite != 'm3d-conversion-host':
                        if fixed_temp.exists():
                            raise RuntimeError(f'task temp path already exists: {fixed_temp}')
                        fixed_temp.mkdir()
                        run_env['TEMP'] = r'C:\a2fo-stage10-final-runtime-temp'
                        run_env['TMP'] = run_env['TEMP']
                        before_temp = captured_files(fixed_temp)
                        if before_temp:
                            raise RuntimeError('new task temp path unexpectedly populated')
                    key = f'{pair}-{version}-run'
                    expected, marker, stream = exit_and_marker(suite, mode)
                    argv = ([str(exe)] if suite == 'm3d-conversion-host' else
                            ['wine', str(exe)])
                    if mode != 'normal':
                        argv.append(mode)
                    try:
                        record = execute(argv, work, key, pins, cwd=runtime,
                                         env=run_env, timeout=240, expected=expected)
                    except Exception:
                        if suite.startswith('m3d-') and suite != 'm3d-conversion-host':
                            archive_temp(fixed_temp, work, pair, version)
                        if lifecycle_root:
                            archive_optional_root(lifecycle_root, work, pair, version)
                        raise
                    captured = work / 'runtime-captured' / pair / version
                    captured.parent.mkdir(parents=True, exist_ok=True)
                    runtime.rename(captured)
                    assets = captured_files(captured)
                    if suite.startswith('m3d-') and suite != 'm3d-conversion-host':
                        assets.update(archive_temp(fixed_temp, work, pair, version))
                    lifecycle_present = None
                    if lifecycle_root:
                        lifecycle_assets, lifecycle_present = archive_optional_root(
                            lifecycle_root, work, pair, version)
                        assets.update(lifecycle_assets)
                    stdout = Path(record['stdout']).read_bytes().replace(b'\r\n', b'\n')
                    stderr = Path(record['stderr']).read_bytes().replace(b'\r\n', b'\n')
                    selected = stdout if stream == 'stdout' else stderr
                    if marker and marker not in selected:
                        raise RuntimeError(f'{key}: required marker absent')
                    if expected == 1 and stderr.strip() != marker:
                        raise RuntimeError(f'{key}: exact expected exception stderr differs')
                    if suite in ('breeders', 'infestation', 'm3d-standalone',
                                 'm3d-builtin') and not stdout and expected == 0:
                        raise RuntimeError(f'{key}: historical fixture produced no trace')
                    state['runs'][key] = {'process': record, 'assets': assets,
                        'lifecycle_root_present': lifecycle_present,
                        'normalized_stdout_sha256': hashlib.sha256(stdout).hexdigest(),
                        'normalized_stderr_sha256': hashlib.sha256(stderr).hexdigest()}
                    traces[version] = (record['exit'], stdout, stderr, assets,
                                       lifecycle_present)
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
