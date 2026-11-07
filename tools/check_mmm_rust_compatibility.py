#!/usr/bin/env python3
"""Build MMM compatibility fixtures; --native explicitly enables headless Wine.

The fixture supplies fake mission/Worker interfaces and a fake edit-mode memory
probe. It never loads the game or executes its absolute-address routines.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = ['before-extend', 'null-extend', 'new-game', 'deferred-finish',
             'detach-null-extend', 'resume', 'repeated-extend', 'construct-retry',
             'start-retry', 'resume-retry', 'shutdown', 'edit-mode']
REFERENCE_SHA = '32830532d404d1235867101be79f5d4f974202151739ca5d0a136f1f31fed9e1'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rust-archive', type=Path, required=True)
    parser.add_argument('--rust-loader', type=Path, required=True)
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    if args.native and args.wine_prefix is None:
        parser.error('--native requires an explicitly reserved private --wine-prefix')
    out = args.output.resolve()
    if not out.is_relative_to(ROOT) or out.exists():
        parser.error('--output must be a new directory inside the repository')
    archive = args.rust_archive.resolve(strict=True)
    loader = args.rust_loader.resolve(strict=True)
    reference = ROOT / 'tests/mmm_reference/module.cpp'
    if sha(reference) != REFERENCE_SHA:
        parser.error('historical lifecycle reference changed')
    inputs = [Path(__file__).resolve(), reference, ROOT / 'modules/A2FOMissions/module.cpp',
              ROOT / 'modules/A2FOMissions/fault_diagnostics.hpp', ROOT / 'sdk/include/a2fo_module_api.h',
              ROOT / 'rust/mmm_runtime_ffi.h', ROOT / 'missions/mmm_loader/shim.cpp',
              ROOT / 'missions/mmm_loader/mmm_loader.def', ROOT / 'tests/mmm_runtime_native.cpp',
              ROOT / 'tests/builtin_missions_bridge_test.cpp', ROOT / 'tests/mmm_runtime_unload.cpp', archive, loader]
    for folder in ['tests/mmm_runtime_mock', 'rust/a2fo_mmm_loader', 'rust/a2fo_core/src']:
        inputs.extend(p for p in (ROOT / folder).rglob('*') if p.is_file())
    out.mkdir(parents=True)
    receipt = {'status': 'running', 'scope': 'fake-host loader and handoff controller only',
               'inputs': {str(p): sha(p) for p in sorted(set(inputs))}, 'steps': [], 'pairs': {}}
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    if args.native:
        env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG='+seh,+process',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
        receipt['native_environment'] = {k: env.get(k) for k in ['WINEPREFIX', 'DISPLAY', 'WAYLAND_DISPLAY', 'WINEDEBUG', 'WINEDLLOVERRIDES']}

    def save():
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')

    def run(name, command, cwd=ROOT, native=False):
        directory = out / 'logs' / name
        directory.mkdir(parents=True)
        item = {'name': name, 'command': list(map(str, command)), 'cwd': str(cwd), 'native': native}
        receipt['steps'].append(item)
        save()
        start = time.monotonic()
        with (directory / 'stdout').open('wb') as stdout, (directory / 'stderr').open('wb') as stderr:
            process = subprocess.run(command, cwd=cwd, env=env if native else None,
                                     stdout=stdout, stderr=stderr, timeout=60 if native else 180)
        item.update(exit=process.returncode, seconds=round(time.monotonic() - start, 3))
        save()
        if process.returncode:
            raise RuntimeError(f'{name}: exit {process.returncode}, see {directory}')
        return (directory / 'stdout').read_bytes()

    def image(path):
        text = subprocess.check_output(['i686-w64-mingw32-objdump', '-p', str(path)], text=True)
        log = out / 'images' / (str(path.relative_to(out)).replace('/', '_') + '.pe')
        log.parent.mkdir(exist_ok=True)
        log.write_text(text)
        exports = {int(a): b for a, b in re.findall(r'^\s*\[\s*\d+\]\s+\+base\[\s*(\d+)\]\s+[0-9a-fA-F]+\s+(\S+)\s*$', text, re.M)}
        dependencies = sorted(re.findall(r'DLL Name:\s*(\S+)', text))
        if not set(x.lower() for x in dependencies) <= {'kernel32.dll', 'msvcrt.dll', 'user32.dll'}:
            raise RuntimeError(f'unexpected fixture import: {path}: {dependencies}')
        return {'sha256': sha(path), 'exports': exports, 'dependencies': dependencies}

    try:
        for executable in ['rustc', 'i686-w64-mingw32-g++']:
            run('version-' + executable, [executable, '--version'])
        host_test = out / 'loader-host-test'
        run('host-test-build', ['rustc', '--edition=2024', '--test', '-O', ROOT / 'rust/a2fo_mmm_loader/lib.rs', '-o', host_test])
        run('host-test-run', [host_test])
        common = ['i686-w64-mingw32-g++', '-std=c++17', '-O2', '-static', '-static-libgcc', '-static-libstdc++']
        for side, source in [('before', reference), ('current', ROOT / 'modules/A2FOMissions/module.cpp')]:
            exe = out / ('runtime-' + side + '.exe')
            command = [*common, '-I', ROOT / 'tests/mmm_runtime_mock', '-I', ROOT / 'modules/A2FOMissions',
                       f'-DA2FO_MMM_MODULE_SOURCE="{source}"', ROOT / 'tests/mmm_runtime_native.cpp']
            if side == 'current':
                command.append(archive)
            run('runtime-build-' + side, [*command, '-o', exe])
            image(exe)
            run('runtime-dll-build-' + side, [*command, '-shared', '-DA2FO_MMM_FIXTURE_DLL=1', '-o', out / ('runtime-' + side + '.dll')])
            image(out / ('runtime-' + side + '.dll'))
        unload_exe = out / 'runtime-unload.exe'
        run('runtime-unload-build', [*common, ROOT / 'tests/mmm_runtime_unload.cpp', '-o', unload_exe])
        image(unload_exe)
        old_loader = out / 'reference-loader.dll'
        run('reference-loader-build', [*common, '-shared', ROOT / 'missions/mmm_loader/shim.cpp',
                                      ROOT / 'missions/mmm_loader/mmm_loader.def', '-o', old_loader])
        current_loader = out / 'rust-loader.dll'
        shutil.copy2(loader, current_loader)
        before, current = image(old_loader), image(current_loader)
        if before['exports'] != current['exports'] or before['dependencies'] != current['dependencies']:
            raise RuntimeError('loader export/ordinal/dependency contract changed')
        if set(current['exports'].values()) != {'ScriptExtendInterface', 'ScriptInit'}:
            raise RuntimeError('unexpected loader exports')
        receipt['loader_boundary'] = {'before': before, 'current': current}
        fixture = ROOT / 'tests/builtin_missions_bridge_test.cpp'
        for mode, define in [('active', '-DTEST_CORE_READY=1'), ('disabled', '-DTEST_CORE_READY=0'),
                             ('old', '-DTEST_OLD_CORE=1'), ('partial', '-DTEST_CORE_MISSING_CALLBACK=1')]:
            directory = out / 'routes' / mode
            directory.mkdir(parents=True)
            core_defines = [define] if mode == 'old' else ['-DTEST_CORE=1', define]
            if mode == 'partial':
                core_defines.append('-DTEST_CORE_READY=1')
            run('core-' + mode, [*common, '-shared', *core_defines, fixture, '-o', directory / 'A2FOExtensions.dll'])
            run('standalone-' + mode, [*common, '-shared', '-DTEST_STANDALONE=1', fixture, '-o', directory / 'A2FOMissions.dll'])
            run('route-exe-' + mode, [*common, fixture, '-o', directory / 'missions_test.exe'])
            image(directory / 'missions_test.exe')
        if args.native:
            for scenario in SCENARIOS:
                outputs = {}
                for side in ['before', 'current']:
                    outputs[side] = run('runtime-' + scenario + '-' + side, ['wine', out / ('runtime-' + side + '.exe'), scenario], out, True)
                if outputs['before'] != outputs['current']:
                    raise RuntimeError('lifecycle transcript mismatch: ' + scenario)
                receipt['pairs'][scenario] = {'exact': True, 'sha256': hashlib.sha256(outputs['current']).hexdigest(), 'lines': len(outputs['current'].splitlines())}
                save()
                print(scenario + ': exact', flush=True)
            for scenario in ['new-game', 'shutdown']:
                outputs = {}
                for side in ['before', 'current']:
                    outputs[side] = run('unload-' + scenario + '-' + side, ['wine', unload_exe, out / ('runtime-' + side + '.dll'), scenario], out, True)
                if outputs['before'] != outputs['current']:
                    raise RuntimeError('FreeLibrary transcript mismatch: ' + scenario)
                receipt['pairs']['unload-' + scenario] = {'exact': True, 'sha256': hashlib.sha256(outputs['current']).hexdigest()}
                save()
            for mode in ['active', 'disabled', 'old', 'partial']:
                directory = out / 'routes' / mode
                for side, source in [('before', old_loader), ('current', current_loader)]:
                    shutil.copy2(source, directory / 'mmm_loader.dll')
                    run('route-' + mode + '-' + side, ['wine', directory / 'missions_test.exe', mode[0]], directory, True)
                receipt['pairs']['route-' + mode] = {'both_exit_zero': True}
                save()
        receipt['inputs_after'] = {name: sha(name) for name in receipt['inputs']}
        if receipt['inputs_after'] != receipt['inputs']:
            raise RuntimeError('test input changed during verification')
        receipt['status'] = 'passed-native' if args.native else 'passed-build-only'
    except Exception as error:
        receipt.update(status='failed', error=repr(error))
        raise
    finally:
        if args.native:
            # Only this explicitly reserved private prefix; never the user's default server.
            for name, command in [('stop', ['wineserver', '-k']), ('wait', ['wineserver', '-w'])]:
                path = out / ('wineserver-' + name + '.log')
                with path.open('wb') as stream:
                    result = subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=30)
                receipt['wineserver_' + name] = {'exit': result.returncode, 'sha256': sha(path)}
        receipt['log_hashes_final'] = {str(p.relative_to(out)): sha(p) for p in sorted((out / 'logs').rglob('*')) if p.is_file()}
        receipt['artifacts'] = {str(p.relative_to(out)): sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in {'.dll', '.exe'}}
        save()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
