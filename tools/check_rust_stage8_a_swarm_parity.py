#!/usr/bin/env python3
"""Compare frozen/current full SwarmSystem native traces under production flags."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BEFORE = ROOT / 'build/rust-migration-stage8/before'
WORK = ROOT / 'build/rust-migration-stage8/runtime-a-swarm-work'
UNIT = Path('modules/A2FOSwarmSystem/module.cpp')
MOTION = Path('modules/A2FOSwarmSystem/swarm_motion.cpp')
HEADER = Path('modules/A2FOSwarmSystem/swarm_motion.hpp')
TEST = ROOT / 'tests/rust_stage8_a_swarm_native.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage8_a_swarm.rs'
FFI = ROOT / 'rust/stage8_a_swarm_ffi.h'
MODES = (
    'init-invalid', 'init-signature', 'init-core-jump', 'init-partial',
    'init-success', 'parse-basic', 'parse-empty', 'parse-zero',
    'parse-no-hierarchy', 'class-throw', 'host-create', 'host-model-fail',
    'host-no-bounds',
    'simulate-pc24', 'simulate-pc53', 'simulate-pc64',
    'simulate-phases', 'simulate-expire',
    'render-accepted', 'render-frustum', 'render-invisible', 'hook-gates',
    'hook-gateway-toggle',
)
FLAVORS = ('standalone', 'builtin')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_fixture(source, motion, archive, exe, flavor):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
        '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source}"',
    ]
    if flavor == 'builtin':
        command += [
            '-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::swarm_system',
            '-DA2FO_ModuleInit=A2FOBuiltinSwarmSystem_Init',
            '-DA2FO_ModuleShutdown=A2FOBuiltinSwarmSystem_Shutdown',
        ]
    command += [str(TEST), str(motion), str(archive), '-ladvapi32',
                '-o', str(exe)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=240)
    if result.returncode:
        raise RuntimeError(f'fixture compile failed: {command}\n'
                           f'{result.stderr[-7000:]!r}')
    return command


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
    if result.returncode:
        raise RuntimeError(f'{exe.name}/{mode} exited {result.returncode}: '
                           f'{output[-4000:]!r} {errors[-4000:]!r}')
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
    WORK.mkdir(parents=True, exist_ok=True)
    archive = args.rust_lib.resolve()
    if not archive.is_file():
        parser.error(f'Rust archive missing: {archive}')
    sources = [BEFORE/UNIT, ROOT/UNIT, BEFORE/MOTION, ROOT/MOTION,
               BEFORE/HEADER, ROOT/HEADER, TEST, Path(__file__), RUST, FFI,
               archive]
    hashes = {str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path):
              digest(path) for path in sources}
    exes = {}
    commands = {}
    for flavor in FLAVORS:
        for version, base in (('before', BEFORE), ('current', ROOT)):
            exe = WORK/f'swarm-{flavor}-{version}.exe'
            command = compile_fixture(base/UNIT, base/MOTION, archive, exe,
                                      flavor)
            exes[(flavor, version)] = exe
            commands[f'{flavor}-{version}'] = command
    if args.build_only:
        print(f'swarm fixture build PASS: {len(exes)} executables; '
              f'{len(MODES)} modes x {len(FLAVORS)} flavors pending')
        return
    comparisons = []
    for flavor in FLAVORS:
        for mode in MODES:
            frozen = run_wine(exes[(flavor, 'before')], mode,
                              args.wine_prefix)
            current = run_wine(exes[(flavor, 'current')], mode,
                               args.wine_prefix)
            (WORK/f'{flavor}-{mode}-before.txt').write_bytes(frozen)
            (WORK/f'{flavor}-{mode}-current.txt').write_bytes(current)
            if frozen != current:
                raise RuntimeError(f'{flavor}/{mode} native trace differs; '
                                   f'see {WORK}')
            comparisons.append({'flavor': flavor, 'mode': mode,
                                'sha256': hashlib.sha256(frozen).hexdigest(),
                                'lines': frozen.count(b'\n')})
    for path in sources:
        key = str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path)
        if digest(path) != hashes[key]:
            raise RuntimeError(f'source/archive drift during test: {path}')
    receipt = {'status': 'passed', 'comparisons': comparisons,
               'count': len(comparisons), 'inputs_sha256': hashes,
               'commands': commands}
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'swarm parity PASS: {len(comparisons)} native comparisons')


if __name__ == '__main__':
    main()
