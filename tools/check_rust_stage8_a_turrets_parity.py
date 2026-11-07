#!/usr/bin/env python3
"""Compare frozen/current full Turrets native source traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BEFORE = ROOT / 'build/rust-migration-stage8/before'
WORK = ROOT / 'build/rust-migration-stage8/runtime-a-turrets-work'
TEST = ROOT / 'tests/rust_stage8_a_turrets_native.cpp'
UNIT = Path('modules/A2FOTurrets/module.cpp')
MATH = ROOT / 'modules/A2FOTurrets/turret_math.cpp'
COMBAT = ROOT / 'modules/A2FOTurrets/turret_combat.cpp'
MATH_HEADER_CPP = ROOT / 'modules/A2FOTurrets/turret_math.hpp'
COMBAT_HEADER_CPP = ROOT / 'modules/A2FOTurrets/turret_combat.hpp'
FROZEN_MATH = BEFORE / 'modules/A2FOTurrets/turret_math.cpp'
FROZEN_COMBAT = BEFORE / 'modules/A2FOTurrets/turret_combat.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage8_a_turrets.rs'
HEADER = ROOT / 'rust/stage8_a_turrets_ffi.h'
RUST_MOTION = ROOT / 'rust/a2fo_core/src/motion.rs'
RUST_COMBAT = ROOT / 'rust/a2fo_core/src/combat_config.rs'
MOTION_HEADER = ROOT / 'rust/motion_ffi.h'
COMBAT_HEADER = ROOT / 'rust/combat_config_ffi.h'
FROZEN_SHA256 = 'ca3120a7eb1f9cd455b25370fa8bfac6f13845afbea1d415959f4e1be41489be'
DEFAULT_LIB = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
MODES = (
    'parse-parent', 'parse-turret', 'classlabel-false',
    'spawn-basic', 'spawn-missing-class', 'spawn-missing-node',
    'spawn-null', 'update-basic', 'update-pc24', 'update-pc53',
    'update-pc64', 'loaded', 'loaded-absent-parent',
    'loaded-missing-mount', 'loaded-duplicate',
    'weapon', 'weapon-match', 'weapon-committed',
    'weapon-no-owner', 'weapon-expired-target',
    'weapon-callback-mutation', 'cleanup', 'cleanup-child-disappeared',
    'event-short',
    'init-invalid', 'init-alias-false', 'init-defaults-false',
    'init-weapon-false', 'init-craft-false', 'init-hook-false',
    'init-patch-false', 'init-success',
)
FLAVORS = ('standalone', 'builtin')
INPUTS = (BEFORE / UNIT, ROOT / UNIT, TEST, MATH, COMBAT,
          MATH_HEADER_CPP, COMBAT_HEADER_CPP, FROZEN_MATH, FROZEN_COMBAT,
          RUST, HEADER, RUST_MOTION, RUST_COMBAT,
          MOTION_HEADER, COMBAT_HEADER)
EXPECTED_MARKERS = {
    'loaded-absent-parent': b'child:12:11:1:',
    'loaded-missing-mount': b'Loaded turret could not resolve its parent mount',
    'loaded-duplicate': b'Duplicate loaded turret detected',
    'weapon': b'weapon:result:0',
    'weapon-match': b'weapon:result:1',
    'weapon-committed': b'weapon:result:1',
    'weapon-no-owner': b'weapon:result:1',
    'weapon-expired-target': b'weapon:result:1',
    'weapon-callback-mutation': b'target:12:13',
    'cleanup': b'craft:expire',
    'cleanup-child-disappeared': b'state:classes=0:parents=0:hosts=0:children=0:targets=0',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pin_inputs(archive):
    return {**{str(path.relative_to(ROOT)): digest(path) for path in INPUTS},
            str(archive): digest(archive)}


def compile_fixture(source, archive, exe, flavor):
    command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
        '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-static', f'-DMODULE_SOURCE="{source}"',
    ]
    if flavor == 'builtin':
        command += [
            '-DA2FO_ModuleInit=A2FOBuiltinTurrets_Init',
            '-DA2FO_ModuleShutdown=A2FOBuiltinTurrets_Shutdown',
        ]
    command += [str(TEST), str(MATH), str(COMBAT), str(archive),
                '-ladvapi32', '-o', str(exe)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=240)
    if result.returncode:
        raise RuntimeError(f'fixture compile failed: {command}\n'
                           f'{result.stderr[-8000:]!r}')


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
    return b'stdout:\n' + output + b'stderr:\n' + errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', DEFAULT_LIB)))
    parser.add_argument('--wine-prefix', type=Path,
        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                    '/tmp/a2fo-shields-wine-prefix')))
    parser.add_argument('--receipt', type=Path, default=WORK / 'parity.json')
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    if digest(BEFORE / UNIT) != FROZEN_SHA256:
        parser.error('frozen Turrets source drift')
    if not archive.is_file():
        parser.error(f'Rust archive missing: {archive}')
    if archive.stat().st_mtime_ns < max(
            path.stat().st_mtime_ns for path in
            (RUST, RUST_MOTION, RUST_COMBAT)):
        parser.error('Rust archive predates Turrets Rust source')
    pinned = pin_inputs(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    sources = {'before': BEFORE / UNIT, 'current': ROOT / UNIT}
    exes = {}
    for flavor in FLAVORS:
        for version, source in sources.items():
            exe = WORK / f'turrets-{flavor}-{version}.exe'
            compile_fixture(source, archive, exe, flavor)
            exes[(flavor, version)] = exe
    if pinned != pin_inputs(archive):
        parser.error('source or Rust archive changed during fixture build')
    if args.build_only:
        print('built four frozen/current Turrets native fixtures')
        return
    comparisons = []
    for flavor in FLAVORS:
        for mode in MODES:
            frozen = run_wine(exes[(flavor, 'before')], mode,
                              args.wine_prefix)
            current = run_wine(exes[(flavor, 'current')], mode,
                               args.wine_prefix)
            for version, trace in (('before', frozen), ('current', current)):
                if not trace.strip() or b'state:classes=' not in trace:
                    raise RuntimeError(f'{flavor}/{mode}/{version} trace empty or missing state')
                marker = EXPECTED_MARKERS.get(mode)
                if marker is not None and marker not in trace:
                    raise RuntimeError(f'{flavor}/{mode}/{version} missing {marker!r}')
            (WORK / f'{flavor}-{mode}-before.txt').write_bytes(frozen)
            (WORK / f'{flavor}-{mode}-current.txt').write_bytes(current)
            if frozen != current:
                raise RuntimeError(f'{flavor}/{mode} native trace mismatch')
            comparisons.append({'flavor': flavor, 'mode': mode,
                                'sha256': hashlib.sha256(frozen).hexdigest()})
    if {(item['flavor'], item['mode']) for item in comparisons} != {
            (flavor, mode) for flavor in FLAVORS for mode in MODES}:
        raise RuntimeError('expected Turrets comparisons missing')
    if pinned != pin_inputs(archive):
        parser.error('source or Rust archive changed during fixture run')
    receipt = {
        'status': 'passed', 'comparisons': comparisons,
        'frozen_sha256': FROZEN_SHA256, 'inputs': pinned,
        'rust_archive': str(archive), 'rust_archive_sha256': digest(archive),
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Turrets: {len(comparisons)} frozen/current native traces matched')


if __name__ == '__main__':
    main()
