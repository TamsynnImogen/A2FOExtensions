#!/usr/bin/env python3
"""Frozen/current whole-source fixtures for the stage-ten core completion slice.

Writes only a fresh fixtures* child of core-completion. Native execution requires
the lead's initialized, private Wine prefix and is deliberately serial.
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
SLICE = ROOT / 'build/rust-migration-stage10/core-completion'
BEFORE = SLICE / 'before'
ORIGINAL = (ROOT / 'build/rust-migration-stage10/render-ui-batch/checkpoint/'
            'artifacts/rust/i686-pc-windows-gnu/release/liba2fo_core.a')
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
CXX = 'i686-w64-mingw32-g++'
CORE_LIBS = ['-ladvapi32', '-lgdi32', '-lcomctl32', '-lversion', '-lws2_32',
             '-lshell32', '-lwinmm', '-lole32', '-lmsimg32', '-lgdiplus', '-ldxguid']
RUST_LIBS = ['-lntdll', '-luserenv', '-lws2_32', '-lbcrypt', '-ladvapi32']
CORE_FLAGS = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
              '-DNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections',
              '-fdata-sections', '-static']
EXTRA_CORE_UNITS = ()
EXTRA_CORE_TRAPS = ()
CORE_CASES = ('settings', 'lifecycle', 'odf', 'dispatch', 'classload',
              'classdefaults', 'lookup', 'config', 'shell', 'parameters', 'nebula',
              'pixels', 'render')
ALL_CASES = (*CORE_CASES, 'menu', 'renderer')
SHARED_FIXTURES = frozenset({'tests/rust_stage10_core_pixels_native.cpp',
                             'tests/rust_stage10_core_render_native.cpp'})
MENU_MODES = ('wide', 'shortcut', 'recent', 'list')
MENU_MARKERS = {
    'wide': (b'stage9 module menu wide: PASS', b'wide-case:C:\\Desktop\\a rather long shortcut.lnk', b'wide-ignored:'),
    'shortcut': (b'stage9 module menu shortcut: PASS', b'shortcut-case:11', b'shortcut-caught:4', b'native:persist-release', b'native:link-release'),
    'recent': (b'stage9 module menu recent: PASS', b'recent-case:15', b'recent-caught:11', b'native:find-close', b'native:co-uninitialize'),
    'list': (b'stage9 module menu list: PASS', b'list-case:3', b'list-end:2:0', b'list-check:'),
}
RENDERER_MARKERS = (
    b'renderer stage9: typed FormShow gateway exception preserved',
    b'renderer stage9: empty, 16 KiB boundary and failed opens passed',
    b'renderer stage9: combo log exception caught before selection',
    b'renderer stage9: reentrant combo admission and priority passed',
)
CORE_MARKERS = {
    'settings': (b'provider:',),
    'lifecycle': (b'core deferred lifecycle: PASS',),
    'odf': (b'core ODF workflow: PASS',),
    'dispatch': (b'trigger-result:', b'craft-result:'),
    'classload': (b'load-case:9', b'context-restored:0', b'core classload fixture: PASS'),
    'classdefaults': (b'alias:1:1234567', b'core classdefaults fixture: PASS'),
    'lookup': (b'registration-result:1:new owner:99', b'core lookup fixture: PASS'),
    'config': (b'case:7:4:0:1', b'wait-sleeps:10000', b'core config fixture: PASS'),
    'shell': (b'case:6:2:3:24', b'core shell fixture: PASS'),
    'parameters': (b'case:5:13', b'core parameter fallback fixture: PASS'),
    'nebula': (b'flat-case:127:5:6', b'release-mutation', b'core nebula fixture: PASS'),
    'pixels': (b'mip-even-odd-pitched:PASS', b'mip-single-preconditions:PASS',
               b'mip-lock-failures:PASS', b'mip-raw-bit-matrix-27:PASS',
               b'load-sparse-pitched:PASS', b'load-dense-tie:PASS',
               b'load-black:PASS', b'load-failures:PASS',
               b'load-sparse-mutation-growth-failure:PASS',
               b'composite-order-cache:PASS', b'composite-loader-fallback:PASS',
               b'core pixels native fixture: PASS'),
    'render': (b'render-command-line:PASS', b'render-path:PASS',
               b'render-suffix:PASS', b'render-gate:PASS',
               b'render-selector:PASS', b'render-compositor:PASS',
               b'stage10 core render native: PASS'),
}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


class InputPins:
    def __init__(self):
        self.digests = {}

    def add(self, path, expected=None):
        path = Path(path).resolve(strict=True)
        actual = sha(path)
        if expected is not None and actual != expected:
            raise RuntimeError(f'input differs from authoritative pin: {path}')
        previous = self.digests.get(str(path))
        if previous is not None and previous != actual:
            raise RuntimeError(f'input changed after first read: {path}')
        self.digests[str(path)] = actual

    def update(self, paths):
        for path in paths:
            self.add(path)

    def verify(self):
        changed = [name for name, digest in self.digests.items()
                   if sha(Path(name)) != digest]
        if changed:
            raise RuntimeError('input drift: ' + ', '.join(changed[:8]))

    def __iter__(self):
        return (Path(name) for name in self.digests)


def run(command, work, key, cwd=ROOT, env=None, timeout=360, pins=None):
    if pins is not None:
        pins.verify()
    out, err = work / f'{key}.stdout', work / f'{key}.stderr'
    with out.open('wb') as stdout, err.open('wb') as stderr:
        try:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=stdout,
                                    stderr=stderr, timeout=timeout, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    record = {'command': list(map(str, command)), 'cwd': str(cwd),
              'returncode': code, 'stdout': str(out), 'stderr': str(err),
              'stdout_sha256': sha(out), 'stderr_sha256': sha(err)}
    (work / f'{key}.json').write_text(json.dumps(record, indent=2) + '\n')
    if pins is not None:
        pins.verify()
    if code != 0:
        raise RuntimeError(f'{key} exit {code}; see {err}')
    return record


def source(version, logical):
    return (BEFORE if version == 'before' else ROOT) / logical


def is_shared_fixture(path):
    return path.is_relative_to(ROOT) and path.relative_to(ROOT).as_posix() in SHARED_FIXTURES


def replace_body(text, signature, body):
    # Preserve the historical settings fixture's inert deferred-wait mock.
    if text.count(signature) != 1:
        raise RuntimeError(f'fixture body marker absent or repeated: {signature}')
    start = text.index(signature) + len(signature)
    level, index, quote, comment = 1, start, None, None
    while level:
        ch, next_char = text[index], text[index + 1:index + 2]
        if comment == 'line':
            if ch == '\n':
                comment = None
        elif comment == 'block':
            if ch == '*' and next_char == '/':
                comment = None
                index += 1
        elif quote:
            if ch == '\\':
                index += 1
            elif ch == quote:
                quote = None
        elif ch == '/' and next_char in ('/', '*'):
            comment = 'line' if next_char == '/' else 'block'
            index += 1
        elif ch in ('"', "'"):
            quote = ch
        elif ch == '{':
            level += 1
        elif ch == '}':
            level -= 1
        index += 1
    return text[:start] + '\n' + body + '\n' + text[index - 1:]


def pinned_before(path, hashes):
    if not path.is_relative_to(BEFORE):
        raise RuntimeError(f'frozen dependency escaped snapshot: {path}')
    name = path.relative_to(BEFORE).as_posix()
    if hashes.get(name) != sha(path):
        raise RuntimeError(f'frozen source lacks matching pin: {name}')


def dependency_scan(command, unit, work, key, pins=None):
    # Scan the exact compiler inputs before compiling; -MM tracks local headers.
    options = command[1:command.index(str(unit))]
    options = [item for item in options if not item.startswith('-Wl,')]
    record = run([CXX, *options, '-MM', str(unit)], work, key, pins=pins)
    output = Path(record['stdout']).read_text().replace('\\\n', ' ')
    if ':' not in output:
        raise RuntimeError(f'empty dependency scan: {key}')
    return {Path(word).resolve() for word in shlex.split(output.split(':', 1)[1])}


def core_fixture(name, version, work):
    if name in ('pixels', 'render'):
        fixture = ROOT / f'tests/rust_stage10_core_{name}_native.cpp'
    else:
        stage = 8 if name in ('settings', 'lifecycle', 'odf', 'dispatch',
                               'classload', 'classdefaults', 'lookup') else 9
        fixture = source(version, f'tests/rust_stage{stage}_core_{name}_native.cpp')
    logical = 'core/nebula_renderer.cpp' if name in ('nebula', 'pixels', 'render') else 'core/dllmain.cpp'
    unit = source(version, logical)
    content = unit.read_text().replace('__declspec(dllexport)', '')
    if name == 'settings':
        content = replace_body(content, 'void wait_for_deferred_initialization() {', 'return;')
    if name not in ('pixels', 'render'):
        content = re.sub(r'\bDllMain\b', 'a2fo_uninvoked_fixture_DllMain', content)
        content = re.sub(r'\bA2FO_Initialize\b', 'a2fo_uninvoked_fixture_Initialize', content)
    if name == 'lookup':
        old = 'void wait_for_deferred_initialization() {'
        if content.count(old) != 1:
            raise RuntimeError('lookup wait fixture marker changed')
        content = content.replace(old, '[[maybe_unused]] void a2fo_fixture_uninvoked_wait() {')
    # The generated source lives outside the original include directory.
    content = re.sub(r'#include "([^"\n]+)"',
                     lambda m: '#include "' + str((unit.parent / m[1]).resolve()) + '"',
                     content)
    generated = work / f'{name}-{version}-source.cpp'
    generated.write_text(content)
    flags = list(CORE_FLAGS)
    if name == 'lifecycle':
        flags += ['-Wno-unused-function', '-Wno-unused-variable']
    if name in ('nebula', 'pixels', 'render'):
        flags += ['-Wno-error=attributes', '-Wno-error=narrowing', '-Wno-unused-function']
    command = [CXX, *flags, f'-DCORE_SOURCE="{generated}"', str(fixture)]
    return command, fixture, generated, unit


def compile_case(name, version, archive, work, hashes, inputs, scans):
    key = f'{name}-{version}'
    if name in CORE_CASES:
        command, fixture, generated, unit = core_fixture(name, version, work)
        inputs.add(generated)
        units = [fixture, generated]
        for path in (fixture, unit):
            if version == 'before' and not is_shared_fixture(path):
                pinned_before(path, hashes)
            inputs.add(path)
        for index, path in enumerate(units):
            deps = dependency_scan([CXX, *command[1:-1], str(path)], path,
                                   work, f'{key}-dep-{index}', pins=inputs)
            scans[f'{key}-{index}'] = sorted(map(str, deps))
            for dep in deps:
                if (version == 'before' and dep.is_relative_to(ROOT)
                        and not dep.is_relative_to(work)
                        and not is_shared_fixture(dep)):
                    pinned_before(dep, hashes)
                inputs.add(dep)
        obj = work / f'{key}.o'
        run([*command, '-c', '-o', str(obj)], work, f'{key}-compile', pins=inputs)
        objects = [obj]
        for extra_index, logical in enumerate(EXTRA_CORE_UNITS):
            helper = source(version, logical)
            if version == 'before':
                pinned_before(helper, hashes)
            inputs.add(helper)
            helper_command = [CXX, *CORE_FLAGS, str(helper)]
            deps = dependency_scan(helper_command, helper, work,
                                   f'{key}-dep-extra-{extra_index}', pins=inputs)
            scans[f'{key}-extra-{extra_index}'] = sorted(map(str, deps))
            for dep in deps:
                if version == 'before' and dep.is_relative_to(ROOT) and not dep.is_relative_to(work):
                    pinned_before(dep, hashes)
                inputs.add(dep)
            helper_obj = work / f'{key}-extra-{extra_index}.o'
            run([*helper_command, '-c', '-o', str(helper_obj)], work,
                f'{key}-compile-extra-{extra_index}', pins=inputs)
            objects.append(helper_obj)
        if name in ('pixels', 'render'):
            helper = source(version, 'core/nebula_emissive.cpp')
            if version == 'before':
                pinned_before(helper, hashes)
            inputs.add(helper)
            helper_command = [CXX, *command[1:-2], str(helper)]
            deps = dependency_scan(helper_command, helper, work,
                                   f'{key}-dep-helper', pins=inputs)
            scans[f'{key}-helper'] = sorted(map(str, deps))
            for dep in deps:
                if version == 'before' and dep.is_relative_to(ROOT) and not dep.is_relative_to(work):
                    pinned_before(dep, hashes)
                inputs.add(dep)
            helper_obj = work / f'{key}-emissive.o'
            run([*helper_command, '-c', '-o', str(helper_obj)], work,
                f'{key}-compile-helper', pins=inputs)
            objects.append(helper_obj)
        undefined = '\n'.join(subprocess.check_output(
            ['i686-w64-mingw32-nm', '--undefined-only', str(item)], text=True)
            for item in objects)
        defined = '\n'.join(subprocess.check_output(
            ['i686-w64-mingw32-nm', '-g', '--defined-only', str(item)], text=True)
            for item in (*objects[1:], archive))
        provided = {line.split()[-1] for line in defined.splitlines() if line.split()}
        native = sorted({line.split()[-1] for line in undefined.splitlines()
                         if line.split() and (line.split()[-1].startswith(
                             ('__ZN4a2fo', '__ZNK4a2fo', '_a2fo_')) or
                             line.split()[-1] in EXTRA_CORE_TRAPS)
                         and not line.split()[-1].startswith('_a2fo_rs_')
                         and line.split()[-1] not in provided})
        stubs = work / f'{key}-uninvoked.S'
        stubs.write_text('.text\n' + ''.join('.globl ' + symbol + '\n' + symbol +
                                            ':\n jmp _a2fo_fixture_unexpected_native\n'
                                            for symbol in native))
        trap = work / f'{key}-trap.cpp'
        trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_fixture_unexpected_native(){std::fputs("unexpected native engine path\\n",stderr);std::abort();}\n')
        inputs.update((stubs, trap))
        link = [CXX, '-static', '-Wl,--gc-sections', *(str(item) for item in objects), str(stubs),
                str(trap), str(archive), *CORE_LIBS]
    else:
        logical = f'core/{"module_menu" if name == "menu" else "renderer_options"}.cpp'
        unit = source(version, logical)
        fixture = source(version, f'tests/rust_stage9_{"module_menu" if name == "menu" else "renderer_options"}_native.cpp')
        for path in (unit, fixture):
            if version == 'before':
                pinned_before(path, hashes)
            inputs.add(path)
        marker = f'#include "../{logical}"'
        original = fixture.read_text()
        if original.count(marker) != 1:
            raise RuntimeError(f'{name} fixture marker changed')
        generated = work / f'{key}-fixture.cpp'
        generated.write_text(original.replace(marker, f'#include "{unit}"'))
        inputs.add(generated)
        if name == 'menu':
            flags = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                     '-Wno-cast-function-type', '-Wno-unused-function', '-DNDEBUG',
                     '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections', '-fdata-sections',
                     '-static', '-static-libgcc', '-static-libstdc++']
            helper = source(version, 'core/extension_roots.cpp')
            if version == 'before':
                pinned_before(helper, hashes)
            inputs.add(helper)
            units = [generated, helper]
        else:
            flags = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                     '-DNDEBUG', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN', '-static',
                     '-static-libgcc', '-static-libstdc++', '-I', str(unit.parent)]
            units = [generated]
        objects = []
        for index, path in enumerate(units):
            command = [CXX, *flags, str(path)]
            deps = dependency_scan(command, path, work, f'{key}-dep-{index}', pins=inputs)
            scans[f'{key}-{index}'] = sorted(map(str, deps))
            for dep in deps:
                if version == 'before' and dep.is_relative_to(ROOT) and not dep.is_relative_to(work):
                    pinned_before(dep, hashes)
                inputs.add(dep)
            obj = work / f'{key}-{index}.o'
            run([*command, '-c', '-o', str(obj)], work,
                f'{key}-compile-{index}', pins=inputs)
            objects.append(obj)
        link = [CXX, '-static', '-static-libgcc', '-static-libstdc++',
                '-Wl,--gc-sections', *(str(p) for p in objects), str(archive)]
        if name == 'menu':
            link += ['-lcomctl32', '-lgdi32', '-lole32', '-lshell32', '-lshlwapi',
                     '-lws2_32', *RUST_LIBS]
        else:
            link += ['-lcomctl32', '-lshlwapi', '-lole32', '-lws2_32', *RUST_LIBS]
    exe = work / f'{key}.exe'
    run([*link, '-o', str(exe)], work, f'{key}-link', pins=inputs)
    return exe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', required=True, type=Path)
    parser.add_argument('--rust-lib', type=Path, default=CANDIDATE)
    parser.add_argument('--build-receipt', required=True, type=Path)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--suites', nargs='+', choices=ALL_CASES,
                        help='selected suites for an explicitly partial probe')
    parser.add_argument('--wine-prefix', type=Path, default=Path('/tmp/a2fo-shields-wine-prefix'))
    parser.add_argument('--fleetops', type=Path, default=ROOT / 'FleetOpsHook.dll')
    args = parser.parse_args()
    selected = tuple(dict.fromkeys(args.suites)) if args.suites else ALL_CASES
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('work directory must be a fresh fixtures* child of core-completion')
    candidate = args.rust_lib.resolve(strict=True)
    original = ORIGINAL.resolve(strict=True)
    receipt_path = args.build_receipt.resolve(strict=True)
    if candidate == original or sha(candidate) == sha(original):
        parser.error('original and candidate archives must be distinct')
    receipt = json.loads(receipt_path.read_text())
    key = candidate.relative_to(ROOT).as_posix()
    if receipt.get('status') != 'passed' or receipt.get('artifacts', {}).get(key) != sha(candidate):
        parser.error('final build receipt does not bind candidate archive')
    build_inputs = receipt.get('inputs_after', {})
    provenance = [ROOT / 'Makefile', ROOT / 'rust/a2fo_core/Cargo.toml',
                  ROOT / 'rust/a2fo_core/Cargo.lock', *ROOT.glob('rust/**/*.rs'),
                  *ROOT.glob('rust/**/*.h')]
    if any(build_inputs.get(p.relative_to(ROOT).as_posix()) != sha(p) for p in provenance):
        parser.error('final build receipt lacks matching Rust/build input pins')
    hashes = json.loads((SLICE / 'before-hashes.json').read_text())
    original_receipt = json.loads((SLICE / 'predecessor-artifacts.json').read_text())
    original_key = ('build/rust-migration-stage10/render-ui-batch/candidate/rust/'
                    'i686-pc-windows-gnu/release/liba2fo_core.a')
    if original_receipt.get(original_key) != sha(original):
        parser.error('preserved original archive hash mismatch')
    if not args.build_only:
        prefix = args.wine_prefix.resolve()
        if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
            parser.error('initialized absolute private Wine prefix required')
        if 'renderer' in selected and not args.fleetops.resolve().is_file():
            parser.error('FleetOpsHook.dll required for renderer fixture')
    work.mkdir(parents=True)
    record = {'status': 'preparing', 'original_archive': str(original),
              'original_sha256': sha(original), 'candidate_archive': str(candidate),
              'candidate_sha256': sha(candidate), 'build_receipt': str(receipt_path),
              'build_receipt_sha256': sha(receipt_path), 'cases': {}, 'dependencies': {},
              'inputs': {}, 'executions': 0, 'trace_pairs': 0,
              'selected_suites': selected, 'full_suite': selected == ALL_CASES,
              'build_only': args.build_only}
    def save():
        (work / 'receipt.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')
    try:
        inputs = InputPins()
        for logical, expected in hashes.items():
            inputs.add(BEFORE / logical, expected)
        for logical, expected in build_inputs.items():
            inputs.add(ROOT / logical, expected)
        for pattern in ('*.cpp', '*.hpp', '*.h'):
            inputs.update((ROOT / 'tests').rglob(pattern))
        inputs.update((original, candidate, receipt_path, Path(__file__).resolve(),
                       SLICE / 'before-hashes.json', SLICE / 'predecessor-artifacts.json',
                       *provenance))
        if 'renderer' in selected and not args.build_only:
            inputs.add(args.fleetops.resolve(strict=True))
        record['prebuild_inputs'] = dict(inputs.digests)
        record['inputs'] = dict(inputs.digests)
        inputs.verify()
        save()
        for name in selected:
            for version, archive in (('before', original), ('current', candidate)):
                exe = compile_case(name, version, archive, work, hashes,
                                   inputs, record['dependencies'])
                record['cases'][f'{name}-{version}'] = {'exe': str(exe), 'sha256': sha(exe)}
                save()
        initial = dict(inputs.digests)
        record['inputs'] = initial
        for path in sorted(inputs):
            relative = path.relative_to(ROOT) if path.is_relative_to(ROOT) else Path('external') / sha(path) / path.name
            target = work / 'source-used' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if sha(target) != initial[str(path)]:
                raise RuntimeError(f'source-used copy mismatch: {path}')
        def freshness():
            inputs.verify()
        freshness()
        record['status'] = 'built' if selected == ALL_CASES else 'built-partial'
        save()
        if args.build_only:
            return
        env = os.environ.copy()
        env.pop('DISPLAY', None); env.pop('WAYLAND_DISPLAY', None)
        env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d',
                   LIBGL_ALWAYS_SOFTWARE='1')
        fleetops = args.fleetops.resolve() if 'renderer' in selected else None
        for name in selected:
            modes = MENU_MODES if name == 'menu' else ('clean', 'roots') if name == 'renderer' else ('default',)
            for mode in modes:
                traces = {}
                for version in ('before', 'current'):
                    freshness()
                    exe = Path(record['cases'][f'{name}-{version}']['exe'])
                    if sha(exe) != record['cases'][f'{name}-{version}']['sha256']:
                        raise RuntimeError(f'executable drift: {exe}')
                    argv = ['wine', str(exe)]
                    if name == 'menu':
                        argv.append(mode)
                    elif name == 'renderer':
                        argv += [str(fleetops), str(fleetops)]
                        if mode == 'clean':
                            argv.append('--clean-layout')
                    runtime = work / 'runtime' / name / mode
                    runtime.mkdir(parents=True, exist_ok=False)
                    result = run(argv, work, f'{name}-{mode}-{version}-run',
                                 cwd=runtime, env=env, timeout=180)
                    captured = work / 'runtime-captured' / name / mode / version
                    captured.parent.mkdir(parents=True, exist_ok=True)
                    runtime.rename(captured)
                    assets = {p.relative_to(captured).as_posix(): sha(p)
                              for p in captured.rglob('*') if p.is_file()}
                    stdout = Path(result['stdout']).read_bytes().replace(b'\r\n', b'\n')
                    stderr = Path(result['stderr']).read_bytes().replace(b'\r\n', b'\n')
                    markers = MENU_MARKERS[mode] if name == 'menu' else RENDERER_MARKERS if name == 'renderer' else CORE_MARKERS[name]
                    if any(marker not in stdout for marker in markers):
                        raise RuntimeError(f'completion marker missing: {name}/{mode}/{version}')
                    if name == 'dispatch' and (stdout.count(b'trigger-result:') != 10 or stdout.count(b'craft-result:') != 3):
                        raise RuntimeError('dispatch case count incomplete')
                    traces[version] = (result['returncode'], stdout, stderr, assets)
                    record['cases'][f'{name}-{version}'].setdefault('runs', {})[mode] = {'result': result, 'runtime_assets': assets}
                    record['executions'] += 1
                    save()
                if traces['before'] != traces['current']:
                    raise RuntimeError(f'frozen/current mismatch: {name}/{mode}')
                record['trace_pairs'] += 1
                save()
        freshness()
        record['status'] = 'pass' if selected == ALL_CASES else 'pass-partial'
        save()
    except Exception as error:
        record['status'] = 'failed'
        record['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    main()
