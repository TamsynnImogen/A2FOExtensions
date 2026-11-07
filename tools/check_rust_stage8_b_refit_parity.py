#!/usr/bin/env python3
"""Build frozen/current real Refit DLLs and compare native console assertions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BEFORE = ROOT / 'build/rust-migration-stage8/before'
WORK = ROOT / 'build/rust-migration-stage8/refit-work'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())
OWNED = [f'modules/A2FORefitMenus/{name}.cpp' for name in
         ('module', 'loading_native', 'setup_native', 'menu_scene')]
OTHER = [f'modules/A2FORefitMenus/{name}.cpp' for name in
         ('presentation', 'skin', 'skin_native', 'scene_catalogue',
          'mission_backdrop', 'scene_composition', 'setup_presentation',
          'loading_gpu', 'loading_content', 'loading_presentation',
          'loading_theme', 'setup_skin')]
MODES = ('missing', 'disabled', 'invalid', 'api', 'identity', 'armada',
         'profile', 'foreign', 'action', 'patch-fail', 'skin-signature',
         'skin-patch-fail', 'success', 'loading', 'loading-signature',
         'mission', 'mission-signature', 'mission-fo', 'mission-fo-gateway')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=180):
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                stderr=err, timeout=timeout)
        out.seek(0); err.seek(0)
        stdout, stderr = out.read(), err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{stdout[-2000:]!r}\n{stderr[-4000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def frozen_source(name):
    original = BEFORE / name
    if sha(original) != HASHES[name]:
        raise RuntimeError(f'frozen hash changed: {name}')
    target = WORK / 'frozen' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    source = original.read_text()
    source = source.replace('#include "../../sdk/', f'#include "{ROOT}/sdk/')
    source = source.replace('#include "../refit_common/',
                            f'#include "{ROOT}/modules/refit_common/')
    target.write_text(source)
    return target

def build_dll(label, archive):
    sources = [frozen_source(name) if label == 'before' else ROOT / name
               for name in OWNED]
    native = [str(path) for path in sources]
    native += [str(ROOT / name) for name in OTHER]
    native += [str(ROOT / 'modules/A2FORefitMenus/native_bridge.S'),
               str(ROOT / 'build/refit_menu_ui_skin.o'),
               str(ROOT / 'build/refit_menu_fonts.o')]
    dll = WORK / f'{label}.dll'
    command = ['i686-w64-mingw32-g++', '-DWIN32_LEAN_AND_MEAN',
               '-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
               '-Werror', '-DNDEBUG', '-DNOMINMAX', '-shared', '-static',
               '-static-libgcc', '-static-libstdc++',
               '-Wl,--enable-stdcall-fixup', '-I',
               str(ROOT / 'modules/A2FORefitMenus'), '-o', str(dll)]
    command += native + ['-lcomctl32', '-lgdi32', '-lmsimg32', '-ladvapi32',
                         '-lgdiplus', str(archive)]
    run(command, timeout=300)
    return dll

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compile-only', action='store_true')
    p.add_argument('--rust-lib', type=Path)
    p.add_argument('--wine-prefix', type=Path)
    p.add_argument('--armada', type=Path, default=ROOT / 'ArmadaL.exe')
    p.add_argument('--fleetops', type=Path, default=ROOT / 'FleetOpsHook.dll')
    args = p.parse_args()
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    for path in (archive, args.armada, args.fleetops,
                 ROOT / 'build/refit_menu_ui_skin.o', ROOT / 'build/refit_menu_fonts.o'):
        if not path.is_file():
            raise RuntimeError(f'required input missing: {path}')
    archive_hash = sha(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    dlls = [build_dll(label, archive) for label in ('before', 'current')]
    fixture = WORK / 'refit_native.exe'
    run(['i686-w64-mingw32-g++', '-DWIN32_LEAN_AND_MEAN',
         '-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-static', '-static-libgcc', '-static-libstdc++',
         '-Wl,--image-base,0x10000000', '-o', str(fixture),
         str(ROOT / 'tests/rust_stage8_b_refit_native.cpp'),
         str(ROOT / 'tests/refit_menu_native_bridge.S'),
         str(ROOT / 'tests/native_reference/adapters/core/hook.cpp'), '-lgdi32', str(archive)], timeout=300)
    if args.compile_only:
        print('refit: frozen/current real DLL and GNU++17/O2/Werror native fixture compile; Wine skipped')
        return
    prefix = args.wine_prefix or Path(os.environ.get('A2FO_RUST_WINEPREFIX',
        WORK / 'wine-prefix'))
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        raise RuntimeError('initialized absolute disposable Wine prefix required')
    env = os.environ.copy()
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', DISPLAY='',
               WAYLAND_DISPLAY='',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    transcripts = []
    for dll in dlls:
        results = []
        for mode in MODES:
            command = ['wine', str(fixture), str(dll), str(args.armada.resolve()),
                       str(args.fleetops.resolve()), mode]
            result = run(command, env=env, timeout=120)
            # The native fixture runs mode-specific loading/mission assertions,
            # then reports their shared successful initialization as "success".
            reported_mode = ('success' if mode.startswith(('loading', 'mission'))
                             else mode)
            if not re.fullmatch(rb'refit menu ' + reported_mode.encode() + rb': PASS\n', result):
                raise RuntimeError(f'unexpected native fixture transcript: {mode} {result!r}')
            results.append(result)
        transcripts.append(b''.join(results))
    if transcripts[0] != transcripts[1]:
        raise RuntimeError('frozen/current Refit native transcripts differ')
    if sha(archive) != archive_hash:
        raise RuntimeError('Rust archive changed during comparison')
    receipt = {'frozen_sources': {name: sha(BEFORE / name) for name in OWNED},
               'current_sources': {name: sha(ROOT / name) for name in OWNED},
               'rust_archive_sha256': archive_hash,
               'fixture_sha256': sha(ROOT / 'tests/rust_stage8_b_refit_native.cpp'),
               'compiler': 'GNU++17/O2/Werror', 'cases': len(MODES),
               'transcript_sha256': hashlib.sha256(transcripts[0]).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'refit: {len(MODES)} frozen/current real-DLL native cases, PASS')

if __name__ == '__main__':
    main()
