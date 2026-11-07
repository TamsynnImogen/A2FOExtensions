#!/usr/bin/env python3
"""Build and compare complete frozen/current QueueEnhancement native mock traces."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UNIT = Path('modules/A2FOFeaturePack/queue_enhancement.cpp')
HEADERS = []
BASE = ROOT/'build/rust-migration-stage8/before'
WORK = ROOT/'build/rust-migration-stage8/queue-work'
RUST = ROOT/'rust/a2fo_core/src/stage8_c_queue.rs'
FIXTURE = ROOT/'tests/rust_stage8_c_queue_mock.cpp'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=240)
    if result.returncode:
        raise RuntimeError(f'{command}: {result.returncode}\n'
                           f'{result.stderr[-6000:]!r}')
    return result.stdout


def capture(executable, env, label):
    stdout = WORK/f'{label}.stdout.log'
    stderr = WORK/f'{label}.stderr.log'
    with stdout.open('wb') as output, stderr.open('wb') as errors:
        result = subprocess.run(['wine', str(executable)], cwd=WORK,
                                env=env, stdout=output, stderr=errors,
                                timeout=240)
    if result.returncode:
        raise RuntimeError(f'{label} exit {result.returncode}: '
                           f'{stderr.read_bytes()[-5000:]!r}; '
                           f'stdout={stdout.read_bytes()[-5000:]!r}')
    return stdout.read_bytes().replace(b'\r\n', b'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
                        default=os.environ.get('A2FO_RUST_WIN_LIB'))
    parser.add_argument('--wine-prefix', type=Path,
                        default=os.environ.get('A2FO_RUST_WINEPREFIX'))
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    hashes = json.loads((ROOT/'build/rust-migration-stage8/before-hashes.json').read_text())
    for relative in [UNIT,*HEADERS]:
        if digest(BASE/relative) != hashes[str(relative)]:
            raise RuntimeError(f'frozen QueueEnhancement source changed: {relative}')
    WORK.mkdir(parents=True, exist_ok=True)
    archive = args.rust_lib or WORK/'libstage8_c_queue.a'
    if args.rust_lib and not archive.exists():
        raise RuntimeError(f'Rust archive missing: {archive}')
    if not args.rust_lib:
        wrapper = WORK/'isolated.rs'
        wrapper.write_text('#![no_std]\nextern crate alloc;\n'
            '#[path = "'+str(ROOT/'rust/a2fo_core/src/allocator.rs')+'"] mod allocator;\n'
            f'#[path = "{RUST}"] mod stage8_c_queue;\n')
        run(['rustc', '--edition=2024', '--target=i686-pc-windows-gnu',
             '--crate-type=staticlib', '-C', 'panic=abort', '-C', 'opt-level=2',
             '-D', 'warnings', str(wrapper), '-o', str(archive)])
    executables = {}
    for label, source in [('before',BASE/UNIT),('current',ROOT/UNIT)]:
        output = WORK/f'queue_{label}.exe'
        command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
                   '-Wall', '-Wextra', '-Werror', '-ffunction-sections',
                   '-fdata-sections', '-Wl,--gc-sections', '-static',
                   '-DA2FO_STAGE8_QUEUE_SOURCE="'+str(source)+'"',
                   str(FIXTURE)]
        if label == 'current':
            command.append(str(archive))
            command += ['-lntdll', '-luserenv', '-lws2_32', '-lbcrypt',
                        '-ladvapi32']
        command += ['-o', str(output)]
        run(command)
        executables[label] = output
    if args.build_only:
        print('queue: complete frozen/current native source fixtures built')
        return
    context = None
    if args.wine_prefix:
        prefix = args.wine_prefix
    else:
        context = tempfile.TemporaryDirectory(prefix='a2fo-stage8-queue-')
        prefix = Path(context.name)/'wine'
    try:
        env = {**os.environ, 'WINEDEBUG':'-all', 'WINEPREFIX':str(prefix),
               'DISPLAY':'', 'WAYLAND_DISPLAY':'',
               'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;'
                                 'winewayland.drv=d;winex11.drv=d'}
        before = capture(executables['before'], env, 'before')
        current = capture(executables['current'], env, 'current')
    finally:
        if context:
            context.cleanup()
    if before != current:
        left, right = before.splitlines(), current.splitlines()
        for index in range(max(len(left),len(right))):
            if index >= len(left) or index >= len(right) or left[index] != right[index]:
                raise RuntimeError(f'first difference line {index+1}: '
                                   f'{left[index:index+1]} != {right[index:index+1]}')
    receipt = {'cases':len(before.splitlines()),
               'sha256':hashlib.sha256(before).hexdigest(),
               'frozen_sha256':digest(BASE/UNIT),
               'current_sha256':digest(ROOT/UNIT),
               'rust_sha256':digest(RUST),
               'rust_archive':str(archive),
               'rust_archive_sha256':digest(archive),'equal':True}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'queue: {receipt["cases"]} lines, {receipt["sha256"]}, PASS')

if __name__ == '__main__':
    main()
