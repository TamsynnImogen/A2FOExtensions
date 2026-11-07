#!/usr/bin/env python3
"""Compare frozen/current CraftIdentity object-editor native workflows headlessly."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/object-editor-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOCraftIdentity/object_editor.cpp'
SOURCE = ROOT / 'modules/A2FOCraftIdentity/object_editor.cpp'
STATE = ROOT / 'modules/A2FOCraftIdentity/object_editor_state.cpp'
FROZEN_STATE = ROOT / 'build/rust-migration-stage8/before/modules/A2FOCraftIdentity/object_editor_state.cpp'
FIXTURE = ROOT / 'tests/rust_stage8_b_object_editor_native.cpp'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())
MODES = ('inactive', 'entry', 'shields', 'load-event')

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
        raise RuntimeError(f'{command}: exit {result.returncode}\n{stdout[-2000:]!r}\n{stderr[-4000:]!r}')
    return stdout.replace(b'\r\n', b'\n')

def compile_fixture(source, label, archive, object_lib):
    fixture = WORK / f'{label}_object_editor_fixture.cpp'
    original = FIXTURE.read_text()
    marker = '#include "../modules/A2FOCraftIdentity/object_editor.cpp"'
    if original.count(marker) != 1:
        raise RuntimeError('object-editor fixture include changed')
    fixture.write_text(original.replace(marker, f'#include "{source}"'))
    target = WORK / f'{label}_object_editor.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
               '-Wextra', '-Wpedantic', '-Werror', '-DNDEBUG', '-DNOMINMAX',
               '-DWIN32_LEAN_AND_MEAN', '-static', '-static-libgcc',
               '-static-libstdc++', str(fixture), str(STATE)]
    if object_lib:
        command.append(str(object_lib))
    command += [str(archive), '-luser32', '-lgdi32', '-ladvapi32', '-lntdll',
                '-luserenv', '-lbcrypt', '-o', str(target)]
    run(command, timeout=300)
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path)
    parser.add_argument('--object-lib', type=Path,
                        help='isolated object-editor policy archive before canonical registration')
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    for path, key in ((FROZEN, 'modules/A2FOCraftIdentity/object_editor.cpp'),
                      (FROZEN_STATE, 'modules/A2FOCraftIdentity/object_editor_state.cpp')):
        if sha(path) != HASHES[key]:
            raise RuntimeError(f'frozen object editor hash changed: {path}')
    if sha(STATE) != sha(FROZEN_STATE):
        raise RuntimeError('shared native object-editor state adapter changed')
    libraries = [archive] + ([args.object_lib] if args.object_lib else [])
    if any(not path.is_file() for path in libraries):
        raise RuntimeError('object-editor Rust archive missing')
    input_hashes = {path: sha(path) for path in
                    (FROZEN, SOURCE, STATE, FIXTURE, *libraries)}
    def check_inputs():
        for path, initial in input_hashes.items():
            if sha(path) != initial:
                raise RuntimeError(f'input changed during object-editor comparison: {path}')
    WORK.mkdir(parents=True, exist_ok=True)
    executables = [compile_fixture(FROZEN, 'frozen', archive, args.object_lib),
                   compile_fixture(SOURCE, 'current', archive, args.object_lib)]
    check_inputs()
    if args.compile_only:
        print('object editor: frozen/current GNU++17/O2/Werror native fixture compile PASS; Wine skipped')
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
        outputs = []
        for mode in MODES:
            output = run(['wine', str(executable), mode], env=env, timeout=120)
            check_inputs()
            if not re.fullmatch(rb'object editor ' + mode.encode() + rb': PASS\n', output):
                raise RuntimeError(f'unexpected {mode} transcript: {output!r}')
            outputs.append(output)
        transcripts.append(b''.join(outputs))
    if transcripts[0] != transcripts[1]:
        raise RuntimeError('frozen/current object-editor transcripts differ')
    check_inputs()
    receipt = {'frozen_source_sha256': sha(FROZEN),
               'current_source_sha256': sha(SOURCE),
               'rust_archive_sha256': sha(archive),
               'object_lib_sha256': sha(args.object_lib) if args.object_lib else None,
               'fixture_sha256': sha(FIXTURE),
               'compiler': 'GNU++17/O2/Werror',
               'cases': len(MODES),
               'transcript_sha256': hashlib.sha256(transcripts[0]).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'object editor: {len(MODES)} frozen/current headless native cases PASS')

if __name__ == '__main__':
    main()
