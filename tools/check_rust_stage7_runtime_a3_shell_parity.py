#!/usr/bin/env python3
"""Build frozen/current ShellDisplay DLLs and exercise the native smoke fixture."""
from pathlib import Path
import os
import subprocess


ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / 'build/rust-migration-stage7/runtime-a-work'
BEFORE = ROOT / 'build/rust-migration-stage7/before'
SOURCE = ROOT / 'modules/A2FOShellDisplay'
RUST = Path(os.environ.get('A2FO_RUST_WIN_LIB', WORK / 'cargo/i686-pc-windows-gnu/release/liba2fo_core.a'))
GAME = ROOT / 'build/engine-compat-20261023/inputs/oct23'


def build(version):
    module = (BEFORE if version == 'before' else ROOT) / 'modules/A2FOShellDisplay/module.cpp'
    output = WORK / f'A2FOShellDisplay-{version}.dll'
    command = ['i686-w64-mingw32-g++', '-DWIN32_LEAN_AND_MEAN', '-std=gnu++17',
               '-O2', '-Wall', '-Wextra', '-Wpedantic', '-DNDEBUG', '-Werror',
               '-shared', '-static', '-static-libgcc', '-static-libstdc++',
               '-Wl,--enable-stdcall-fixup', '-o', str(output), str(module),
               str(SOURCE / 'bink_video.cpp'), str(SOURCE / 'native_intro.cpp'),
               str(SOURCE / 'canvas.cpp'), str(SOURCE / 'delphi_bridge.S')]
    # Native intro already calls the shared Rust policy in the frozen build.
    command.append(str(RUST))
    command.extend(('-lgdi32', '-luser32', '-ladvapi32', '-lgdi32'))
    subprocess.run(command, cwd=ROOT, check=True)
    return output


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
    subprocess.run(['make', 'build/shell_display_native_smoke.exe'], cwd=ROOT, check=True)
    environment = os.environ.copy()
    environment.pop('DISPLAY', None); environment.pop('WAYLAND_DISPLAY', None)
    environment.update(WINEPREFIX=str(os.environ.get('A2FO_RUST_WINEPREFIX', WORK / 'wine-prefix')), WINEDEBUG='-all',
                       WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    for version in ('before', 'current'):
        dll = build(version)
        result = wine_capture(['wine', str(ROOT / 'build/shell_display_native_smoke.exe'),
                                 str(dll), str(GAME / 'ArmadaL.exe'),
                                 str(GAME / 'FleetOpsHook.dll')],
                                env=environment, text=True, timeout=60, check=False)
        if result.returncode or 'Shell display native smoke: PASS' not in result.stdout:
            raise RuntimeError(f'{version}: {result.returncode}\n{result.stdout}\n{result.stderr}')
        print(f'A2FOShellDisplay {version}: native fixture PASS', flush=True)
    print('2/2 native fixture processes passed')


if __name__ == '__main__':
    main()
