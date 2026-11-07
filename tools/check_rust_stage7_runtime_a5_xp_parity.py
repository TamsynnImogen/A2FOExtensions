#!/usr/bin/env python3
"""Compare actual frozen/current CollectiveXP module traces under 32-bit Wine."""
from pathlib import Path
import difflib
import os
import subprocess

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / 'build/rust-migration-stage7/runtime-a-work'
BEFORE = ROOT / 'build/rust-migration-stage7/before'
LIB = Path(os.environ.get('A2FO_RUST_WIN_LIB', WORK / 'cargo/i686-pc-windows-gnu/release/liba2fo_core.a'))
FLAGS = ['-DWIN32_LEAN_AND_MEAN', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
         '-Wpedantic', '-DNDEBUG', '-Werror', '-static', '-static-libgcc', '-static-libstdc++']


def run(args, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)


def wine_capture(args, env, timeout=120, text=False, check=True):
    import tempfile
    print('+', ' '.join(map(str, args)), flush=True)
    mode = 'w+t' if text else 'w+b'
    with tempfile.TemporaryFile(mode=mode) as stdout, tempfile.TemporaryFile(mode=mode) as stderr:
        result = subprocess.run(args, cwd=ROOT, env=env, stdout=stdout, stderr=stderr,
                                timeout=timeout)
        stdout.seek(0); stderr.seek(0)
        output, errors = stdout.read(), stderr.read()
    if check and result.returncode:
        raise RuntimeError(f'Wine fixture exited {result.returncode}: {args}\n{output}\n{errors}')
    return subprocess.CompletedProcess(args, result.returncode, output, errors)

def main():
    env = os.environ.copy()
    env.pop('DISPLAY', None); env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(os.environ.get('A2FO_RUST_WINEPREFIX', WORK / 'wine-prefix'))
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = 'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
    outputs = []
    for version in ('before', 'current'):
        source = (BEFORE if version == 'before' else ROOT) / 'modules/A2FOCollectiveXP/module.cpp'
        exe = WORK / f'xp-{version}.exe'
        run(['i686-w64-mingw32-g++', *FLAGS,
             *(['-DTEST_CURRENT'] if version == 'current' else []),
             f'-DMODULE_SOURCE="{source}"',
             '-o', str(exe), str(ROOT / 'tests/rust_stage7_runtime_a5_xp_parity.cpp'),
             str(ROOT / 'modules/A2FOCollectiveXP/collective_xp.cpp'),
             str(ROOT / 'modules/A2FOCollectiveXP/native_bridge.S'), str(LIB),
             '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32'])
        result = wine_capture(['wine', str(exe)], env=env, timeout=120)
        (WORK / f'xp-{version}.txt').write_bytes(result.stdout)
        outputs.append(result.stdout)
    if outputs[0] != outputs[1]:
        print(''.join(difflib.unified_diff(
            outputs[0].decode(errors='replace').splitlines(True),
            outputs[1].decode(errors='replace').splitlines(True),
            fromfile='frozen', tofile='current')))
        raise RuntimeError('CollectiveXP frozen/current mismatch')
    print(f'CollectiveXP: {len(outputs[0].splitlines())} matching trace lines', flush=True)


if __name__ == '__main__':
    main()
