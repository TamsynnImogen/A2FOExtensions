#!/usr/bin/env python3
"""Frozen/current headless D3D8 device and factory differential oracle."""
import hashlib
import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/rust-migration-stage7/before'
WORK = ROOT / 'build/rust-migration-stage7/device-work'
RESOURCE_WORK = ROOT / 'build/rust-migration-stage7/resources-work'
HASHES = json.loads((ROOT / 'build/rust-migration-stage7/before-hashes.json').read_text())
UNITS = [Path('renderer/d3d8_compat/game/device.cpp'),
         Path('renderer/d3d8_compat/game/factory.cpp'),
         Path('renderer/d3d8_compat/game/resources.cpp')]
HEADER = Path('renderer/d3d8_compat/game/private.hpp')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, env=None):
    p = subprocess.run(command, cwd=ROOT, capture_output=True,
                       env=env, timeout=180)
    if p.returncode:
        raise RuntimeError(f'exit {p.returncode}: {command}\n'
                           f'{p.stderr[-3000:]!r}\n{p.stdout[-1000:]!r}')
    return p.stdout.replace(b'\r\n', b'\n')


def compile_pair(fixture, support, units, tag, archive):
    outputs = []
    for label, root in [('before', BASE), ('current', ROOT)]:
        exe = WORK / f'{tag}-{label}.exe'
        command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
                   '-ffunction-sections', '-fdata-sections',
                   '-Wl,--gc-sections', '-static', str(ROOT / fixture)]
        command += [str(p) for p in support]
        command += [str(root / unit) for unit in units]
        if label == 'current': command.append(str(archive))
        command += ['-ldxguid', '-luuid', '-o', str(exe)]
        run(command)
        outputs.append(exe)
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path, help='link current fixtures against this Rust archive')
    parser.add_argument('--isolated', action='store_true', help='build the standalone Rust module archive')
    parser.add_argument('--wine-prefix', type=Path, help='reuse this prewarmed isolated Wine prefix')
    args = parser.parse_args()
    canonical = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
    selected = args.rust_lib or (canonical if canonical.exists() and not args.isolated else None)
    for path in [*UNITS, HEADER]:
        if sha(BASE / path) != HASHES[str(path)]:
            raise RuntimeError(f'frozen hash mismatch: {path}')
    if sha(ROOT / HEADER) != HASHES[str(HEADER)]:
        raise RuntimeError('private.hpp changed since freeze')
    WORK.mkdir(parents=True, exist_ok=True)
    wrapper = WORK / 'standalone_combined.rs'
    wrapper.write_text(
        '#![no_std]\nunsafe extern "C" { fn abort() -> !; }\n'
        '#[panic_handler] fn panic(_: &core::panic::PanicInfo) -> ! '
        '{ unsafe { abort() } }\n'
        '#[unsafe(no_mangle)] extern "C" fn rust_eh_personality('
        '_v:i32,_a:i32,_c:u64,_o:*mut core::ffi::c_void,'
        '_x:*mut core::ffi::c_void)->i32 { unsafe { abort() } }\n'
        'mod resources { include!("'+str(ROOT/'rust/a2fo_core/src/game_resources.rs')+'"); }\n'
        'mod device { include!("'+str(ROOT/'rust/a2fo_core/src/game_device.rs')+'"); }\n')
    archive = selected or WORK / 'libgame_combined.a'
    if selected:
        if not archive.is_file(): raise RuntimeError(f'Rust archive missing: {archive}')
    else:
        run(['rustc', '--edition=2024', '--target', 'i686-pc-windows-gnu',
             '-C', 'panic=abort', '-D', 'unsafe-op-in-unsafe-fn',
             '--crate-type', 'staticlib', str(wrapper), '-o', str(archive)])
    # The math bodies are extracted unmodified and compiled in separate TUs.
    for label, root in [('before', BASE), ('current', ROOT)]:
        source = (root / UNITS[0]).read_text()
        math = source[source.index('void identity('):source.index('Device::Device(')]
        (WORK / f'{label}_math.cpp').write_text(
            '#include <windows.h>\n#include <algorithm>\n#include <iterator>\n'
            '#include "'+str(WORK/'math_mock.hpp')+'"\n'
            '#include "'+str(ROOT/'rust/game_device_ffi.h')+'"\n'
            'namespace a2fo::renderer::game {\n'+math+'}\n')
    math_exes=[]
    for label in ('before','current'):
        exe=WORK/f'math-{label}.exe'
        cmd=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-static',
             str(ROOT/'tests/rust_stage7_device_parity.cpp'),
             str(WORK/f'{label}_math.cpp')]
        if label=='current':cmd.append(str(archive))
        cmd+=['-o',str(exe)]
        run(cmd)
        math_exes.append(exe)
    device_exes=compile_pair(
        Path('tests/rust_stage7_device_mock.cpp'),
        [WORK/'device_support.cpp', WORK/'device_missing_virtuals.cpp'],
        UNITS,'device',archive)
    factory_exes=compile_pair(
        Path('tests/rust_stage7_factory_mock.cpp'),
        [WORK/'factory_support.cpp', RESOURCE_WORK/'full_support.cpp',
         RESOURCE_WORK/'device_virtuals.cpp'],
        [UNITS[1],UNITS[2]],'factory',archive)
    results={}
    with (nullcontext(args.wine_prefix) if args.wine_prefix else tempfile.TemporaryDirectory(prefix='a2fo-stage7-game-wine-')) as temp:
        env=os.environ.copy()
        prefix=Path(temp) if args.wine_prefix else Path(temp)/'wine'
        env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',
                   DISPLAY='',WAYLAND_DISPLAY='',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                    'winewayland.drv=d;winex11.drv=d')
        for tag,pair in [('matrix',math_exes),('device',device_exes),
                         ('factory',factory_exes)]:
            before,current=[run(['wine',str(exe)],env) for exe in pair]
            if before!=current:
                a,b=before.splitlines(),current.splitlines()
                for i in range(max(len(a),len(b))):
                    if i>=len(a) or i>=len(b) or a[i]!=b[i]:
                        raise RuntimeError(f'{tag} line {i+1}: '
                                           f'{a[i:i+1]} != {b[i:i+1]}')
            results[tag]={'lines':len(before.splitlines()),
                          'sha256':hashlib.sha256(before).hexdigest(),
                          'equal':True}
    receipt={'frozen':{str(p):HASHES[str(p)] for p in UNITS},
             'current':{str(p):sha(ROOT/p) for p in UNITS},
             'rust_sha256':sha(ROOT/'rust/a2fo_core/src/game_device.rs'),
             'rust_archive':str(archive),'rust_archive_sha256':sha(archive),
             'fixture_sha256':{p.name:sha(p) for p in [
                 ROOT/'tests/rust_stage7_device_parity.cpp',
                 ROOT/'tests/rust_stage7_device_mock.cpp',
                 ROOT/'tests/rust_stage7_factory_mock.cpp']},
             'results':results}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    for name,item in results.items():
        print(f"{name}: {item['lines']} traces, {item['sha256']}, PASS")

if __name__=='__main__':main()
