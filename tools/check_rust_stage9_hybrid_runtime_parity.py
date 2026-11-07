#!/usr/bin/env python3
"""Build and compare frozen/current actual-source hybrid menu runtime fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/hybrid-work'
UNIT = ROOT / 'modules/A2FOHybridBuild/hybrid_production_runtime.cpp'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOHybridBuild/hybrid_production_runtime.cpp'
HASHES = ROOT / 'build/rust-migration-stage8/before-hashes.json'
FIXTURE = ROOT / 'tests/rust_stage9_hybrid_runtime_native.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_hybrid_runtime.rs'
HEADER = ROOT / 'rust/stage9_hybrid_runtime_ffi.h'
SCRIPT = Path(__file__).resolve()
DEFAULT_BASE = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
HELPERS = ('hybrid_production.cpp', 'build_submenu_config.cpp')
HELPER_HEADERS = ('hybrid_production.hpp', 'build_submenu_config.hpp')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(argv, label, phase):
    stdout = WORK / f'{label}-{phase}.stdout'
    stderr = WORK / f'{label}-{phase}.stderr'
    with stdout.open('wb') as out, stderr.open('wb') as err:
        result = subprocess.run(argv, cwd=WORK, stdout=out, stderr=err,
                                timeout=300)
    if result.returncode:
        raise RuntimeError(f'{label} {phase} exit {result.returncode}: '
                           f'{stderr.read_bytes()[-5000:]!r}')


def fixture_source(source):
    text = source.read_text().replace('__declspec(dllexport)', '')
    text = text.replace('a2fo::startup_module_provider(',
                        '::fixture_startup_provider(')
    return re.sub(r'#include "([^"\n]+)"',
                  lambda match: '#include "' +
                  str((source.parent / match[1]).resolve()) + '"', text)


def compile_fixture(label, source, base, new=None):
    generated = WORK / f'{label}-hybrid.cpp'
    generated.write_text(fixture_source(source))
    obj = WORK / f'{label}.o'
    exe = WORK / f'{label}.exe'
    flags = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
             '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-Wno-error=cast-function-type', '-Wno-unused-function',
             '-Wno-unused-variable', '-Wno-unused-parameter',
             '-Wno-error=mismatched-new-delete', '-DNDEBUG',
             '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections',
             '-fdata-sections', '-static']
    command(flags + [f'-DHYBRID_SOURCE="{generated}"', str(FIXTURE),
                     '-c', '-o', str(obj)], label, 'compile')
    helper_objects = []
    defined = set()
    for helper in HELPERS:
        helper_obj = WORK / f'{label}-{Path(helper).stem}.o'
        command(flags + ['-c', str(source.parent / helper), '-o',
                         str(helper_obj)], label, f'{Path(helper).stem}-compile')
        helper_objects.append(helper_obj)
        symbols = subprocess.check_output(
            ['i686-w64-mingw32-nm', '--defined-only', str(helper_obj)], text=True)
        defined.update(line.split()[-1] for line in symbols.splitlines()
                       if line.split())
    undefined = subprocess.check_output(
        ['i686-w64-mingw32-nm', '--undefined-only', str(obj)], text=True)
    native = []
    for line in undefined.splitlines():
        name = line.split()[-1]
        if name.startswith(('_a2fo_', '__ZN4a2fo', '__ZNK4a2fo')) and \
           not name.startswith('_a2fo_rs_') and name not in defined:
            native.append(name)
    stubs = WORK / f'{label}-uninvoked.S'
    stubs.write_text('.text\n' + ''.join(
        '.globl ' + name + '\n' + name + ':\n'
        f' pushl $.Lhybrid_trap_{index}\n'
        ' call _a2fo_hybrid_fixture_unexpected_native\n'
        ' ud2\n'
        f'.section .rdata\n.Lhybrid_trap_{index}:\n'
        f' .asciz {json.dumps(name)}\n.text\n'
        for index, name in enumerate(native)))
    trap = WORK / 'uninvoked.cpp'
    trap.write_text('#include <cstdlib>\n#include <cstdio>\n'
                    'extern "C" [[noreturn]] void '
                    'a2fo_hybrid_fixture_unexpected_native(const char* symbol){'
                    'std::fprintf(stderr,"unexpected native Hybrid engine path: %s\\n",symbol);'
                    'std::abort();}\n')
    archives = ([str(new)] if new else []) + [str(base)]
    command(['i686-w64-mingw32-g++', '-static', '-Wl,--gc-sections',
             str(obj), *map(str, helper_objects), str(stubs), str(trap)] + archives +
            ['-ladvapi32', '-lgdi32', '-lcomctl32', '-lversion',
             '-lws2_32', '-lshell32', '-lwinmm', '-lole32',
             '-lmsimg32', '-lgdiplus', '-ldxguid', '-o', str(exe)],
            label, 'link')
    return exe


def run_native(exe, prefix, mode):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    out = WORK / f'{exe.stem}-{mode}.stdout'
    err = WORK / f'{exe.stem}-{mode}.stderr'
    arguments = [] if mode == 'normal' else [mode]
    with out.open('wb') as stdout, err.open('wb') as stderr:
        result = subprocess.run(['wine', str(exe), *arguments], cwd=WORK, env=env,
                                stdout=stdout, stderr=stderr, timeout=240)
    expected = 0 if mode == 'normal' else 77
    if result.returncode != expected:
        raise RuntimeError(f'{exe.name} exit {result.returncode}: '
                           f'{err.read_bytes()[-4000:]!r}')
    return out.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_BASE)))
    parser.add_argument('--rust-lib', type=Path,
                        help='isolated Hybrid archive for build-only pre-integration checks')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    WORK / 'wine-prefix')))
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    base = args.base_lib.resolve()
    new = args.rust_lib.resolve() if args.rust_lib else None
    hashes = json.loads(HASHES.read_text())
    frozen_inputs = (FROZEN,) + tuple(FROZEN.parent / helper for helper in
                                      HELPERS + HELPER_HEADERS)
    for frozen_input in frozen_inputs:
        key = str(frozen_input.relative_to(ROOT / 'build/rust-migration-stage8/before'))
        if digest(frozen_input) != hashes[key]:
            parser.error(f'frozen Hybrid source drift: {key}')
    if not base.is_file() or (new is not None and not new.is_file()):
        parser.error('Rust archive missing')
    defined = subprocess.check_output(
        ['i686-w64-mingw32-nm', '--defined-only', str(base)], text=True)
    integrated = any(line.split()[-1].lstrip('_') ==
                     'a2fo_rs_stage9_hybrid_runtime'
                     for line in defined.splitlines() if line.split())
    if new is None:
        if not integrated:
            parser.error('canonical archive lacks Hybrid runtime export')
        if base.stat().st_mtime_ns < max(RUST.stat().st_mtime_ns,
                                        HEADER.stat().st_mtime_ns):
            parser.error('canonical archive predates Hybrid Rust source/header')
    else:
        if integrated:
            parser.error('base archive already has Hybrid runtime; omit --rust-lib')
        if not args.build_only:
            parser.error('isolated Hybrid overlay is build-only; use canonical archive for native parity')
        if new.stat().st_mtime_ns < max(RUST.stat().st_mtime_ns,
                                       HEADER.stat().st_mtime_ns):
            parser.error('isolated archive predates Hybrid Rust source/header')
    helper_inputs = tuple(directory / helper for directory in
                          (UNIT.parent, FROZEN.parent)
                          for helper in HELPERS + HELPER_HEADERS)
    inputs = (UNIT, FROZEN, FIXTURE, RUST, HEADER, SCRIPT, base, *helper_inputs) + \
        ((new,) if new is not None else ())
    initial = {str(path): digest(path) for path in inputs}
    WORK.mkdir(parents=True, exist_ok=True)
    before = compile_fixture('before', FROZEN, base)
    current = compile_fixture('current', UNIT, base, new)
    if {str(path): digest(path) for path in inputs} != initial:
        raise RuntimeError('Hybrid source/archive changed during build')
    receipt = {'mode': 'build-only' if args.build_only else 'native-parity',
               'inputs_sha256': initial,
               'before_sha256': digest(before),
               'current_sha256': digest(current)}
    if not args.build_only:
        prefix = args.wine_prefix.resolve()
        if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
            parser.error('initialized absolute private Wine prefix required')
        receipt['transcripts'] = {}
        for mode in ('normal', 'log-failure'):
            original = run_native(before, prefix, mode)
            migrated = run_native(current, prefix, mode)
            if {str(path): digest(path) for path in inputs} != initial:
                raise RuntimeError('Hybrid source/archive changed during run')
            if original != migrated:
                old, new_lines = original.splitlines(), migrated.splitlines()
                for index in range(max(len(old), len(new_lines))):
                    if old[index:index+1] != new_lines[index:index+1]:
                        raise RuntimeError(f'Hybrid {mode} difference line '
                                           f'{index+1}: {old[index:index+1]!r} '
                                           f'!= {new_lines[index:index+1]!r}')
                raise RuntimeError(f'Hybrid {mode} transcript differs')
            receipt['transcripts'][mode] = {
                'sha256': hashlib.sha256(original).hexdigest(),
                'lines': len(original.splitlines())}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
