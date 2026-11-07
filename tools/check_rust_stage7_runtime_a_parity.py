#!/usr/bin/env python3
"""Compile frozen/current module sources into headless 32-bit mock runners."""
from pathlib import Path
import difflib
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / 'build/rust-migration-stage7/runtime-a-work'
BEFORE = ROOT / 'build/rust-migration-stage7/before'
LIB = Path(os.environ.get('A2FO_RUST_WIN_LIB', WORK / 'cargo/i686-pc-windows-gnu/release/liba2fo_core.a'))
TEST = ROOT / 'tests/rust_stage7_runtime_a_parity.cpp'
CXX = 'i686-w64-mingw32-g++'

def run(command, **kwargs):
    print('+', ' '.join(map(str, command)), flush=True)
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)

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
    WORK.mkdir(parents=True, exist_ok=True)
    if not LIB.is_file():
        raise RuntimeError(f'missing isolated Rust library: {LIB}')
    for module in ('A2FOWreckage', 'A2FOInfoIni'):
      for flavor in ('standalone', 'builtin'):
        outputs = []
        for version in ('before', 'current'):
            source = (BEFORE if version == 'before' else ROOT) / 'modules' / module / 'module.cpp'
            exe = WORK / f'{module}-{flavor}-{version}.exe'
            command = [CXX, '-std=gnu++17', '-O2', '-DNDEBUG', '-static',
                       f'-DMODULE_SOURCE="{source}"',
                       '-DTEST_WRECKAGE' if module == 'A2FOWreckage' else '-DTEST_INFO',
                       str(TEST)]
            if flavor == 'builtin':
                feature = 'wreckage' if module == 'A2FOWreckage' else 'info_ini'
                command += ['-DTEST_BUILTIN',
                            f'-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::{feature}']
            if module == 'A2FOWreckage' and version == 'before':
                command.append(str(BEFORE / 'modules/A2FOWreckage/wreckage_policy.cpp'))
            if module == 'A2FOInfoIni':
                command.append(str(ROOT / 'core/extension_roots.cpp'))
            command += [str(LIB), '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32', '-o', str(exe)]
            run(command)
            env = os.environ.copy()
            env.pop('DISPLAY', None); env.pop('WAYLAND_DISPLAY', None)
            env['WINEPREFIX'] = str(os.environ.get('A2FO_RUST_WINEPREFIX', WORK / 'wine-prefix'))
            env['WINEDEBUG'] = '-all'
            env['WINEDLLOVERRIDES'] = 'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
            result = wine_capture(['wine', str(exe)], env=env, text=True, timeout=120)
            transcript = WORK / f'{module}-{flavor}-{version}.txt'
            transcript.write_text(result.stdout)
            outputs.append(result.stdout)
        if outputs[0] != outputs[1]:
            print(''.join(difflib.unified_diff(outputs[0].splitlines(True),
                                               outputs[1].splitlines(True),
                                               fromfile='frozen', tofile='current')))
            return 1
        print(f'{module} {flavor}: frozen/current traces match ({len(outputs[0].splitlines())} lines)')
    return 0

if __name__ == '__main__':
    sys.exit(main())
