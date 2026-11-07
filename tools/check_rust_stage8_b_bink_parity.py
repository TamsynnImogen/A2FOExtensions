#!/usr/bin/env python3
"""Compare frozen/current Shell Bink workflows using the headless native mock."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/bink-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOShellDisplay/bink_video.cpp'
SOURCE = ROOT / 'modules/A2FOShellDisplay/bink_video.cpp'
FIXTURE = ROOT / 'tests/shell_intro_test.cpp'
INTRO = ROOT / 'modules/A2FOShellDisplay/native_intro.cpp'
ARMADA = ROOT / 'ArmadaL.exe'
FLEETOPS = ROOT / 'FleetOpsHook.dll'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=180):
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                    stderr=err, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            out.seek(0); err.seek(0)
            raise RuntimeError(f'{command}: timed out after {timeout}s\n'
                               f'stdout={out.read()[-2000:]!r}\n'
                               f'stderr={err.read()[-4000:]!r}') from exc
        out.seek(0); err.seek(0)
        stdout, stderr = out.read(), err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n'
                           f'{stdout[-2000:]!r}\n{stderr[-4000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def compile_fixture(source, label, archive, bink_lib):
    fixture = WORK / f'{label}_bink_native.cpp'
    original = FIXTURE.read_text()
    marker = '#include "../modules/A2FOShellDisplay/bink_video.cpp"'
    if original.count(marker) != 1:
        raise RuntimeError('Bink native fixture include changed')
    header = '#include "../modules/A2FOShellDisplay/bink_video.hpp"'
    if original.count(header) != 1:
        raise RuntimeError('Bink native fixture header include changed')
    fixture.write_text(original.replace(header,
        f'#include "{source.parent / "bink_video.hpp"}"').replace(marker,
        f'#include "{source}"'))
    target = WORK / f'{label}_bink_native.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
               '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG', '-DNOMINMAX',
               '-DWIN32_LEAN_AND_MEAN', '-static', '-static-libgcc',
               '-static-libstdc++', str(fixture), str(INTRO)]
    if bink_lib:
        command.append(str(bink_lib))
    command += [str(archive), '-lgdi32', '-luser32', '-ladvapi32', '-lntdll',
                '-luserenv', '-lbcrypt', '-lws2_32', '-lole32', '-lshell32',
                '-lshlwapi', '-lcomctl32', '-o', str(target)]
    run(command, timeout=300)
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path)
    parser.add_argument('--bink-lib', type=Path,
                        help='isolated Bink archive before canonical registration')
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    if sha(FROZEN) != HASHES['modules/A2FOShellDisplay/bink_video.cpp']:
        raise RuntimeError('frozen Bink source changed')
    libraries = [archive] + ([args.bink_lib] if args.bink_lib else [])
    if any(not path.is_file() for path in libraries):
        raise RuntimeError('Bink Rust archive missing')
    inputs = (FROZEN, SOURCE, FIXTURE, INTRO, ARMADA, FLEETOPS, *libraries)
    input_hashes = {path: sha(path) for path in inputs}
    def check_inputs():
        for path, initial in input_hashes.items():
            if sha(path) != initial:
                raise RuntimeError(f'input changed during Bink comparison: {path}')
    WORK.mkdir(parents=True, exist_ok=True)
    executables = [compile_fixture(FROZEN, 'frozen', archive, args.bink_lib),
                   compile_fixture(SOURCE, 'current', archive, args.bink_lib)]
    check_inputs()
    if args.compile_only:
        print('Bink: frozen/current GNU++17/O2/Werror native fixture compile PASS; Wine skipped')
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
    for executable in executables:
        output = run(['wine', str(executable), str(ARMADA), str(FLEETOPS)],
                     env=env, timeout=120)
        check_inputs()
        if not re.fullmatch(rb'shell intro synthetic window, startup calls and renderer forwarding passed\n', output):
            raise RuntimeError(f'unexpected Bink transcript: {output!r}')
        transcripts.append(output)
    if transcripts[0] != transcripts[1]:
        raise RuntimeError('frozen/current Bink transcripts differ')
    check_inputs()
    receipt = {'frozen_source_sha256': sha(FROZEN),
               'current_source_sha256': sha(SOURCE),
               'rust_archive_sha256': sha(archive),
               'bink_lib_sha256': sha(args.bink_lib) if args.bink_lib else None,
               'fixture_sha256': sha(FIXTURE),
               'armada_sha256': sha(ARMADA), 'fleetops_sha256': sha(FLEETOPS),
               'compiler': 'GNU++17/O2/Werror, assertions enabled',
               'cases': 1,
               'transcript_sha256': hashlib.sha256(transcripts[0]).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Bink: frozen/current headless native workflow PASS')

if __name__ == '__main__':
    main()
