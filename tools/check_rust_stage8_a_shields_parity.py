#!/usr/bin/env python3
"""Compare frozen/current complete AlwaysShowShields native mock traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/runtime-a-shields-work'
BEFORE = ROOT / 'build/rust-migration-stage8/before'
UNIT = Path('modules/A2FOAlwaysShowShields/module.cpp')
TEST = ROOT / 'tests/rust_stage8_a_shields_native.cpp'
BRIDGE = ROOT / 'modules/A2FOAlwaysShowShields/thiscall_bridge.S'
VISIBILITY = ROOT / 'modules/A2FOAlwaysShowShields/shield_visibility.cpp'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_compile(command):
    result = subprocess.run(command, cwd=ROOT, stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE, timeout=180)
    if result.returncode:
        raise RuntimeError(f'compile exited {result.returncode}: {command}\n'
                           f'{result.stderr[-6000:]!r}')


def run_wine(command, prefix, case_root):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['A2FO_SHIELDS_ROOT'] = case_root
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
    )
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                stderr=stderr, timeout=120)
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().replace(b'\r\n', b'\n')
        errors = stderr.read()
    if result.returncode:
        raise RuntimeError(f'Wine fixture exited {result.returncode}: {command}\n'
                           f'{output[-4000:]!r}\n{errors[-4000:]!r}')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
                        default=os.environ.get('A2FO_RUST_WIN_LIB',
                                               ROOT/'build/rust/i686-pc-windows-gnu/'
                                               'release/liba2fo_core.a'))
    parser.add_argument('--wine-prefix', type=Path,
                        default=os.environ.get('A2FO_RUST_WINEPREFIX',
                                               '/tmp/a2fo-shields-wine-prefix'))
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--receipt', type=Path, default=WORK/'parity.json')
    args = parser.parse_args()
    frozen = BEFORE / UNIT
    pinned = json.loads((ROOT/'build/rust-migration-stage8/before-hashes.json').read_text())
    if digest(frozen) != pinned[str(UNIT)]:
        raise RuntimeError('frozen AlwaysShowShields source changed')
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        raise RuntimeError(f'missing Rust archive: {archive}')
    WORK.mkdir(parents=True, exist_ok=True)
    results = {}
    for flavor in ('standalone', 'builtin'):
        for version in ('before', 'current'):
            source = frozen if version == 'before' else ROOT / UNIT
            exe = WORK / f'shields-{flavor}-{version}.exe'
            command = [
                'i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
                '-Wextra', '-Wpedantic', '-Werror', '-ffunction-sections',
                '-fdata-sections', '-Wl,--gc-sections', '-static',
                f'-DMODULE_SOURCE="{source}"', str(TEST), str(BRIDGE),
            ]
            if flavor == 'builtin':
                command += ['-DTEST_BUILTIN',
                            '-DA2FO_BUILTIN_FEATURE='
                            'a2fo::BuiltinFeature::always_show_shields']
            if version == 'before':
                command.append(str(VISIBILITY))
            command += [str(archive), '-lws2_32', '-lbcrypt', '-lole32',
                        '-ladvapi32', '-o', str(exe)]
            run_compile(command)
            results[(flavor, version)] = exe
    if args.build_only:
        print('AlwaysShowShields frozen/current standalone+builtin fixtures built')
        return
    traces = {}
    drive_c = args.wine_prefix / 'drive_c'
    if not drive_c.is_dir():
        raise RuntimeError(f'missing Wine C drive: {drive_c}')
    with tempfile.TemporaryDirectory(prefix='a2fo-stage8-shields-',
                                     dir=drive_c) as host_root:
        host_root = Path(host_root)
        case_dir = host_root / 'root1' / 'odf'
        case_dir.mkdir(parents=True)
        (case_dir/'CASE.odf').write_bytes(b'alwaysShowShields = yes\n')
        (case_dir/'case.odf').write_bytes(b'alwaysShowShields = no\n')
        case_root = 'C:\\' + host_root.name
        for flavor in ('standalone', 'builtin'):
            for mode in ('signature', 'first', 'second', 'third', 'root-throw',
                         'success', 'gateway', 'render-list', 'render-activation'):
                for version in ('before', 'current'):
                    output = run_wine(
                        ['wine', str(results[(flavor, version)]), mode],
                        args.wine_prefix, case_root)
                    (WORK/f'{flavor}-{mode}-{version}.txt').write_bytes(output)
                    traces[(flavor, mode, version)] = output
                frozen_trace = traces[(flavor, mode, 'before')]
                current_trace = traces[(flavor, mode, 'current')]
                if frozen_trace != current_trace:
                    raise RuntimeError(f'AlwaysShowShields {flavor}/{mode} mismatch: '
                                       f'{frozen_trace!r} != {current_trace!r}')
    receipt = {
        'unit': str(UNIT),
        'cases': len(traces)//2,
        'lines': sum(len(traces[(flavor, mode, 'before')].splitlines())
                     for flavor in ('standalone', 'builtin')
                     for mode in ('signature', 'first', 'second', 'third', 'root-throw',
                                  'success', 'gateway', 'render-list', 'render-activation')),
        'frozen_sha256': digest(frozen),
        'current_sha256': digest(ROOT/UNIT),
        'rust_sha256': digest(ROOT/'rust/a2fo_core/src/stage8_a.rs'),
        'ffi_sha256': digest(ROOT/'rust/stage8_a_ffi.h'),
        'fixture_sha256': digest(TEST),
        'runner_sha256': digest(Path(__file__)),
        'rust_archive': str(archive),
        'rust_archive_sha256': digest(archive),
        'equal': True,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2)+'\n')
    print(f'AlwaysShowShields: {receipt["cases"]} frozen/current native traces PASS')


if __name__ == '__main__':
    main()
