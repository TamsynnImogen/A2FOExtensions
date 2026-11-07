#!/usr/bin/env python3
"""Compare frozen/current full Squadrons native module traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/runtime-a-squadrons-work'
BEFORE = ROOT / 'build/rust-migration-stage8/before'
UNIT = Path('modules/A2FOSquadrons/module.cpp')
TEST = ROOT / 'tests/rust_stage8_a_squadrons_native.cpp'
INL_UNITS = (Path('modules/A2FOSquadrons/native_pod_selection.inl'),
             Path('modules/A2FOSquadrons/native_repair.inl'),
             Path('modules/A2FOSquadrons/native_economics.inl'))
MODES = (
    'init-short', 'init-no-armada', 'init-no-fleet',
    'init-signature', 'init-finish-first', 'init-finish-second',
    'init-success', 'init-hook-repair-fail',
    'init-hook-selection-fail', 'init-register-fail',
    'init-armada-throw', 'init-class-throw',
    'init-register-badalloc', 'init-hook-throw',
    'init-yard-first-fail', 'init-yard-second-fail',
    'fields', 'member-resolver', 'class-events', 'class-malformed',
    'econ-pc24', 'econ-pc53', 'econ-pc64', 'econ-reentrant',
    'producer-events', 'craft-events', 'selection-basic',
    'selection-capacity', 'selection-mutation', 'pod-selection',
    'pod-duplicate',
    'repair-observe', 'repair-update', 'repair-queue',
    'repair-queue-drop', 'repair-cancel-reenter',
    'finish-repair', 'finish-repair-cancelled',
    'bar-pc24', 'bar-pc53', 'bar-pc64',
    'ui-dedup', 'panel-off', 'panel-active', 'panel-nested',
    'poison-off',
    'launch-sequence', 'launch-first-fail',
    'launch-second-fail', 'launch-yard-cleanup',
    'finish-ordinary', 'finish-squad', 'finish-spawn-fail',
)

FLAVORS = ('standalone', 'builtin')
EXPECTED_EXCEPTION = {'init-hook-throw'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_fixture(source, archive, exe, flavor):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
        '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source}"',
    ]
    if flavor == 'builtin':
        command += ['-DA2FO_ModuleInit=A2FOBuiltinSquadrons_Init',
                    '-DA2FO_ModuleShutdown=A2FOBuiltinSquadrons_Shutdown']
    command += [
        str(TEST), str(ROOT/'modules/A2FOSquadrons/squadron_config.cpp'),
        str(ROOT/'modules/A2FOSquadrons/squadron_state.cpp'),
        str(ROOT/'modules/A2FOSquadrons/thiscall_bridge.S'),
        str(archive), '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32',
        '-o', str(exe),
    ]
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=240)
    if result.returncode:
        raise RuntimeError(f'fixture compile failed: {command}\n'
                           f'{result.stderr[-7000:]!r}')


def run_wine(exe, mode, prefix):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = (
        'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
    )
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        result = subprocess.run(['wine', str(exe), mode], cwd=ROOT, env=env,
                                stdout=stdout, stderr=stderr, timeout=180)
        stdout.seek(0)
        stderr.seek(0)
        output = stdout.read().replace(b'\r\n', b'\n')
        errors = stderr.read().replace(b'\r\n', b'\n')
    if result.returncode and mode not in EXPECTED_EXCEPTION:
        raise RuntimeError(f'{exe.name}/{mode} exited {result.returncode}: '
                           f'{output[-4000:]!r} {errors[-4000:]!r}')
    if result.returncode == 0 and mode in EXPECTED_EXCEPTION:
        raise RuntimeError(f'{exe.name}/{mode} did not propagate native exception')
    return (f'exit:{result.returncode}\n'.encode() + b'stdout:\n' + output +
            b'stderr:\n' + errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
                        default=os.environ.get('A2FO_RUST_WIN_LIB',
                                               ROOT/'build/rust/i686-pc-windows-gnu/'
                                               'release/liba2fo_core.a'))
    parser.add_argument('--wine-prefix', type=Path,
                        default=os.environ.get('A2FO_RUST_WINEPREFIX',
                                               '/tmp/a2fo-shields-wine-prefix'))
    parser.add_argument('--receipt', type=Path, default=WORK/'parity.json')
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    frozen = BEFORE/UNIT
    pinned = json.loads((ROOT/'build/rust-migration-stage8/before-hashes.json').read_text())
    if digest(frozen) != pinned[str(UNIT)]:
        raise RuntimeError('frozen Squadrons module source changed')
    for path in INL_UNITS:
        if digest(BEFORE/path) != pinned[str(path)]:
            raise RuntimeError(f'frozen Squadrons include changed: {path}')
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        raise RuntimeError(f'missing Rust archive: {archive}')
    WORK.mkdir(parents=True, exist_ok=True)
    exes = {}
    for flavor in FLAVORS:
        for version in ('before', 'current'):
            source = frozen if version == 'before' else ROOT/UNIT
            exe = WORK/f'squadrons-{flavor}-{version}.exe'
            compile_fixture(source, archive, exe, flavor)
            exes[(flavor, version)] = exe
    if args.build_only:
        print('Squadrons frozen/current standalone+builtin module fixtures built')
        return
    results = {}
    for flavor in FLAVORS:
        for mode in MODES:
            for version in ('before', 'current'):
                output = run_wine(exes[(flavor, version)], mode,
                                  args.wine_prefix)
                (WORK/f'{flavor}-{mode}-{version}.txt').write_bytes(output)
                results[(flavor, mode, version)] = output
            if results[(flavor, mode, 'before')] != results[(flavor, mode, 'current')]:
                raise RuntimeError(f'Squadrons {flavor}/{mode} frozen/current mismatch')
    receipt = {
        'unit': str(UNIT),
        'cases': len(MODES) * len(FLAVORS),
        'lines': sum(len(results[(flavor, mode, 'before')].splitlines())
                     for flavor in FLAVORS for mode in MODES),
        'frozen_sha256': digest(frozen),
        'frozen_inl_sha256': {str(x): digest(BEFORE/x) for x in INL_UNITS},
        'current_inl_sha256': {str(x): digest(ROOT/x) for x in INL_UNITS},
        'current_sha256': digest(ROOT/UNIT),
        'rust_sha256': digest(ROOT/'rust/a2fo_core/src/stage8_a_squadrons.rs'),
        'ffi_sha256': digest(ROOT/'rust/stage8_a_squadrons_ffi.h'),
        'fixture_sha256': digest(TEST),
        'runner_sha256': digest(Path(__file__)),
        'rust_archive': str(archive),
        'rust_archive_sha256': digest(archive),
        'equal': True,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Squadrons: {receipt["cases"]} frozen/current native traces PASS')


if __name__ == '__main__':
    main()
