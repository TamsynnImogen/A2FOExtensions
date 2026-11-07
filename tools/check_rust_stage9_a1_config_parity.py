#!/usr/bin/env python3
"""Compare the frozen/current A1 config workflows using actual module source."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/a1-config-work'
UNIT = ROOT / 'modules/A1Compat/module.cpp'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A1Compat/module.cpp'
BASE_HASHES = ROOT / 'build/rust-migration-stage8/before-hashes.json'
FIXTURE = ROOT / 'tests/rust_stage9_a1_config_native.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_a1_config.rs'
HEADER = ROOT / 'rust/stage9_a1_config_ffi.h'
SCRIPT = Path(__file__).resolve()
DEFAULT_LIB = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(argv, stdout, stderr, timeout=300):
    with stdout.open('wb') as out, stderr.open('wb') as err:
        result = subprocess.run(argv, cwd=WORK, stdout=out, stderr=err,
                                timeout=timeout)
    if result.returncode:
        raise RuntimeError(f'{argv[0]} exited {result.returncode}: '
                           f'{stderr.read_bytes()[-5000:]!r}')


def compile_fixture(label, source, base_archive, new_archive=None):
    text = source.read_text().replace('__declspec(dllexport)', '')
    text = re.sub(r'#include "([^"\n]+)"',
                  lambda match: '#include "' +
                  str((source.parent / match[1]).resolve()) + '"', text)
    generated = WORK / f'{label}-a1-config.cpp'
    generated.write_text(text)
    obj = WORK / f'{label}.o'
    exe = WORK / f'{label}.exe'
    flags = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
             '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-Wno-error=cast-function-type',
             '-Wno-error=mismatched-new-delete',
             '-Wno-error=unused-function', '-DNDEBUG',
             '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections',
             '-fdata-sections', '-static']
    command(flags + [f'-DA1_SOURCE="{generated}"', str(FIXTURE),
                     '-c', '-o', str(obj)],
            WORK / f'{label}-compile.stdout',
            WORK / f'{label}-compile.stderr')
    undefined = subprocess.check_output(
        ['i686-w64-mingw32-nm', '--undefined-only', str(obj)], text=True)
    native = []
    for line in undefined.splitlines():
        name = line.split()[-1]
        if name.startswith(('_a2fo_a1_', '__ZN8a1compat', '__ZNK8a1compat')):
            native.append(name)
    stubs = WORK / f'{label}-uninvoked.S'
    stubs.write_text('.text\n' + ''.join(
        '.globl ' + name + '\n' + name + ':\n'
        ' jmp _a2fo_a1_fixture_unexpected_native\n' for name in native))
    trap = WORK / 'uninvoked.cpp'
    trap.write_text('#include <cstdlib>\n#include <cstdio>\n'
                    'extern "C" [[noreturn]] void '
                    'a2fo_a1_fixture_unexpected_native(){'
                    'std::fputs("unexpected native A1 engine path\\n",stderr);'
                    'std::abort();}\n')
    archives = ([str(new_archive)] if new_archive else []) + [str(base_archive)]
    command(flags + ['-Wl,--gc-sections', str(obj), str(stubs),
                     str(trap)] + archives +
            ['-ladvapi32', '-lgdi32', '-lcomctl32', '-lversion',
             '-lws2_32', '-lshell32', '-lwinmm', '-lole32',
             '-lmsimg32', '-lgdiplus', '-ldxguid', '-o', str(exe)],
            WORK / f'{label}-link.stdout',
            WORK / f'{label}-link.stderr')
    return exe


def run_native(exe, prefix):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    out = WORK / f'{exe.stem}.stdout'
    err = WORK / f'{exe.stem}.stderr'
    with tempfile.TemporaryDirectory(prefix=f'{exe.stem}-', dir=WORK) as directory:
        with out.open('wb') as stdout, err.open('wb') as stderr:
            result = subprocess.run(['wine', str(exe)], cwd=directory, env=env,
                                    stdout=stdout, stderr=stderr, timeout=240)
    if result.returncode:
        raise RuntimeError(f'{exe.name} exited {result.returncode}: '
                           f'{err.read_bytes()[-4000:]!r}')
    return out.read_bytes()


def main():
    global WORK
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_LIB)))
    parser.add_argument('--rust-lib', type=Path,
                        help='Optional isolated stage-nine archive; omit for canonical archive')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    '/tmp/a2fo-shields-wine-prefix')))
    parser.add_argument('--work-dir', type=Path, default=WORK,
                        help='isolated build and trace directory')
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    WORK = args.work_dir.resolve()
    base = args.base_lib.resolve()
    new = args.rust_lib.resolve() if args.rust_lib else None
    expected = json.loads(BASE_HASHES.read_text())['modules/A1Compat/module.cpp']
    if digest(FROZEN) != expected:
        parser.error('frozen A1 source drift')
    if not base.is_file() or (new is not None and not new.is_file()):
        parser.error('Rust archive missing')
    if new is not None and new.stat().st_mtime_ns < max(
            RUST.stat().st_mtime_ns, HEADER.stat().st_mtime_ns):
        parser.error('isolated archive predates A1 source/header')
    if new is None:
        defined = subprocess.check_output(
            ['i686-w64-mingw32-nm', '--defined-only', str(base)], text=True)
        if not any(line.split()[-1].lstrip('_') ==
                   'a2fo_rs_stage9_a1_config_gameplay'
                   for line in defined.splitlines() if line.split()):
            parser.error('canonical archive lacks A1 config gameplay export')
    inputs = (UNIT, FROZEN, FIXTURE, RUST, HEADER, SCRIPT, base) + \
        ((new,) if new is not None else ())
    initial = {str(path): digest(path) for path in inputs}
    WORK.mkdir(parents=True, exist_ok=True)
    before = compile_fixture('before', FROZEN, base)
    current = compile_fixture('current', UNIT, base, new)
    if {str(path): digest(path) for path in inputs} != initial:
        raise RuntimeError('A1 config source/archive changed during build')
    receipt = {'mode': 'build-only' if args.build_only else 'native-parity',
               'archive_mode': 'isolated' if new is not None else 'canonical',
               'inputs_sha256': initial,
               'before_sha256': digest(before),
               'current_sha256': digest(current)}
    if not args.build_only:
        prefix = args.wine_prefix.resolve()
        prefix.mkdir(parents=True, exist_ok=True)
        original = run_native(before, prefix)
        migrated = run_native(current, prefix)
        if {str(path): digest(path) for path in inputs} != initial:
            raise RuntimeError('A1 config source/archive changed during run')
        required = (b'literal:-0.5:', b'literal:99999999.5:',
                    b'gui-all:', b'gui-gaps:', b'gui-primary:',
                    b'gui-high-root-exception:')
        if any(marker not in original for marker in required) or \
                original.count(b'ini-roots:') != 4:
            raise RuntimeError('frozen A1 config transcript omitted expanded cases')
        if original != migrated:
            old, new_lines = original.splitlines(), migrated.splitlines()
            for index in range(max(len(old), len(new_lines))):
                if old[index:index+1] != new_lines[index:index+1]:
                    raise RuntimeError(f'A1 config difference line {index+1}: '
                                       f'{old[index:index+1]!r} != '
                                       f'{new_lines[index:index+1]!r}')
            raise RuntimeError('A1 config transcript differs')
        receipt['transcript_sha256'] = hashlib.sha256(original).hexdigest()
        receipt['lines'] = len(original.splitlines())
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
