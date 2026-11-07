#!/usr/bin/env python3
"""Frozen/current whole-source module-menu comparison using inert native operations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/module-menu-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/core/module_menu.cpp'
SOURCE = ROOT / 'core/module_menu.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_module_menu.rs'
HEADER = ROOT / 'rust/stage9_module_menu_ffi.h'
HELPER = ROOT / 'core/extension_roots.cpp'
FIXTURE = ROOT / 'tests/rust_stage9_module_menu_native.cpp'
MODES = ('wide', 'shortcut', 'recent', 'list')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=300):
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                    stderr=err, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            out.seek(0); err.seek(0)
            raise RuntimeError(f'{command}: timed out after {timeout}s\n'
                               f'stdout={out.read()[-2000:]!r}\n'
                               f'stderr={err.read()[-5000:]!r}') from exc
        out.seek(0); err.seek(0)
        stdout, stderr = out.read(), err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{stdout[-3000:]!r}\n{stderr[-7000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def prepared(label, source):
    output = WORK / f'{label}-fixture.cpp'
    marker = '#include "../core/module_menu.cpp"'
    content = FIXTURE.read_text()
    if content.count(marker) != 1:
        raise RuntimeError('fixture source include changed')
    output.write_text(content.replace(marker, f'#include "{source}"'))
    return output

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--rust-object', type=Path,
                        help='prebuilt isolated object for build-only when the archive lacks the export')
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    hashes = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())
    if sha(FROZEN) != hashes['core/module_menu.cpp']:
        raise RuntimeError('frozen module-menu source changed')
    archive = Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve()
    if not archive.is_file():
        raise RuntimeError(f'canonical archive missing: {archive}')
    exports = run(['i686-w64-mingw32-nm', '-g', '--defined-only', str(archive)])
    canonical = b'_a2fo_rs_stage9_module_menu_flow' in exports
    if not args.build_only and not canonical:
        raise RuntimeError('native parity requires a canonical archive with module-menu flow')
    pinned = [FROZEN, SOURCE, RUST, HEADER, HELPER,
              ROOT / 'core/extension_roots.hpp', ROOT / 'rust/roots_ffi.h',
              ROOT / 'rust/a2fo_core/src/roots.rs', ROOT / 'rust/a2fo_core/src/lib.rs',
              ROOT / 'build/rust-migration-stage8/before/core/module_menu.hpp',
              ROOT / 'core/module_menu.hpp', FIXTURE, Path(__file__).resolve(), archive,
              ROOT / 'build/rust-migration-stage8/before-hashes.json']
    isolated = None
    if not canonical:
        isolated = (args.rust_object or WORK / 'stage9-module-menu-flow.o').resolve()
        if args.rust_object:
            if not isolated.is_file(): raise RuntimeError(f'isolated Rust object missing: {isolated}')
            pinned.append(isolated)
    initial = {str(path): sha(path) for path in pinned}
    if isolated is not None and not args.rust_object:
        run(['rustc', '--target', 'i686-pc-windows-gnu', '--edition=2024',
             '--crate-type', 'lib', '--emit=obj', '-C', 'panic=abort', '-C', 'opt-level=2',
             str(RUST), '-o', str(isolated)])
    def freshness():
        for path in pinned:
            if sha(path) != initial[str(path)]:
                raise RuntimeError(f'module-menu input changed during parity: {path}')
    executables = []
    flags = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
             '-Wpedantic', '-Werror', '-Wno-cast-function-type', '-Wno-unused-function',
             '-DNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections', '-fdata-sections',
             '-static', '-static-libgcc', '-static-libstdc++']
    for label, source in [('frozen', FROZEN), ('current', SOURCE)]:
        obj = WORK / f'{label}.o'
        exe = WORK / f'{label}.exe'
        run(flags + ['-c', str(prepared(label, source)), '-o', str(obj)])
        run(flags + ['-c', str(HELPER), '-o', str(WORK / f'{label}-roots.o')])
        # The fixture supplies only inert native mocks. The real roots helper is
        # linked for exact /mod parsing, while all actual desktop operations are intercepted.
        libraries = [str(isolated)] if isolated is not None else []
        run(['i686-w64-mingw32-g++', '-static', '-Wl,--gc-sections', str(obj),
             str(WORK / f'{label}-roots.o'), *libraries, str(archive),
             '-lcomctl32', '-lgdi32', '-lole32', '-lshell32', '-lshlwapi',
             '-lws2_32', '-lntdll', '-luserenv', '-lbcrypt', '-ladvapi32',
             '-o', str(exe)])
        executables.append(exe)
    freshness()
    if args.build_only:
        mode = 'canonical' if canonical else 'isolated'
        print(f'module-menu {mode} frozen/current GNU++17/O2/Werror whole-source build PASS; Wine skipped')
        return
    prefix = args.wine_prefix or Path(os.environ.get('A2FO_RUST_WINEPREFIX',
        '/tmp/a2fo-shields-wine-prefix'))
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        raise RuntimeError('initialized absolute private Wine prefix required')
    env = os.environ.copy()
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', DISPLAY='', WAYLAND_DISPLAY='',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    transcripts = []
    for label, exe in zip(('frozen', 'current'), executables):
        output = b''
        for mode in MODES:
            case = run(['wine', str(exe), mode], env=env, timeout=120)
            expected = f'stage9 module menu {mode}: PASS\n'.encode()
            if expected not in case or not re.search(rb'^(?:wide|shortcut|recent|list)-case:', case, re.M):
                raise RuntimeError(f'{label} {mode} marker missing: {case[-1000:]!r}')
            required = {
                'wide': (b'wide-case:C:\\Desktop\\a rather long shortcut.lnk', b'wide-ignored:'),
                'shortcut': (b'shortcut-case:11', b'shortcut-caught:4', b'native:persist-release', b'native:link-release'),
                'recent': (b'recent-case:15', b'recent-caught:11', b'native:find-close', b'native:co-uninitialize'),
                'list': (b'list-case:3', b'list-end:2:0', b'list-check:'),
            }[mode]
            if any(marker not in case for marker in required):
                raise RuntimeError(f'{label} {mode} scenario marker missing: {case[-1000:]!r}')
            (WORK / f'{label}-{mode}.txt').write_bytes(case)
            output += case
            freshness()
        transcripts.append(output)
        (WORK / f'{label}.txt').write_bytes(output)
    if transcripts[0] != transcripts[1]:
        old, new = (value.splitlines() for value in transcripts)
        for index in range(max(len(old), len(new))):
            if old[index:index+1] != new[index:index+1]:
                raise RuntimeError(f'module-menu difference line {index+1}: '
                                   f'{old[index:index+1]} != {new[index:index+1]}')
        raise RuntimeError('module-menu transcript mismatch')
    freshness()
    receipt = {'inputs': initial, 'rust_link_mode': 'canonical' if canonical else 'isolated',
               'modes': MODES, 'lines': len(transcripts[0].splitlines()),
               'transcript_sha256': hashlib.sha256(transcripts[0]).hexdigest(),
               'compiler': 'GNU++17/O2/Werror', 'native_scope': 'inert COM/Win32/listview mocks'}
    if isolated is not None:
        receipt['isolated_rust_object_sha256'] = sha(isolated)
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'module-menu frozen/current native traces: {receipt["lines"]} lines PASS')

if __name__ == '__main__':
    main()
