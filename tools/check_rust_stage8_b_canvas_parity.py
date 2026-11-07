#!/usr/bin/env python3
"""Compare frozen/current Shell Canvas under four synthetic native hosts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage8/canvas-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOShellDisplay/canvas.cpp'
SOURCE = ROOT / 'modules/A2FOShellDisplay/canvas.cpp'
HEADER = ROOT / 'modules/A2FOShellDisplay/canvas.hpp'
FROZEN_HEADER = ROOT / 'build/rust-migration-stage8/before/modules/A2FOShellDisplay/canvas.hpp'
ASM = ROOT / 'modules/A2FOShellDisplay/delphi_bridge.S'
HASHES = json.loads((ROOT / 'build/rust-migration-stage8/before-hashes.json').read_text())
TESTS = {
    'lifecycle': ('tests/shell_canvas_lifecycle_test.cpp',
                  rb'Shell canvas lifecycle: PASS \(synthetic USER32 host, no HWNDs\)\n'),
    'dialog': ('tests/shell_canvas_dialog_test.cpp',
               rb'shell native dialog nested input dispatch passed\n'),
    'scroll': ('tests/shell_canvas_scroll_test.cpp',
               rb'shell canvas scroll, cached rows and dirty paint passed\n'),
    'copy': ('tests/shell_canvas_copy_test.cpp',
             rb'shell canvas copy export passed\n'),
}

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

def compile_fixture(source, label, case, fixture_path, archive, canvas_lib):
    original = fixture_path.read_text()
    marker = '#include "../modules/A2FOShellDisplay/canvas.cpp"'
    header = '#include "../modules/A2FOShellDisplay/canvas.hpp"'
    sdk = '#include "../sdk/include/a2fo_shell_canvas.h"'
    if original.count(marker) != 1 or original.count(header) != 1:
        raise RuntimeError(f'{case} Canvas fixture include changed')
    original = original.replace(header, f'#include "{source.parent / "canvas.hpp"}"')
    original = original.replace(marker, f'#include "{source}"')
    original = original.replace(sdk, f'#include "{ROOT / "sdk/include/a2fo_shell_canvas.h"}"')
    fixture = WORK / f'{label}_{case}_fixture.cpp'
    fixture.write_text(original)
    target = WORK / f'{label}_{case}.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall',
               '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG', '-DNOMINMAX',
               '-DWIN32_LEAN_AND_MEAN', '-static', '-static-libgcc',
               '-static-libstdc++', str(fixture), str(ASM)]
    if canvas_lib:
        command.append(str(canvas_lib))
    command += [str(archive), '-lgdi32', '-luser32', '-ladvapi32', '-lntdll',
                '-luserenv', '-lbcrypt', '-lws2_32', '-lole32', '-lshell32',
                '-lshlwapi', '-lcomctl32', '-o', str(target)]
    run(command, timeout=300)
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path)
    parser.add_argument('--canvas-lib', type=Path,
                        help='isolated Canvas rlib before canonical registration')
    parser.add_argument('--wine-prefix', type=Path)
    args = parser.parse_args()
    archive = args.rust_lib or Path(os.environ.get('A2FO_RUST_WIN_LIB',
        ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'))
    if sha(FROZEN) != HASHES['modules/A2FOShellDisplay/canvas.cpp'] or \
            sha(FROZEN_HEADER) != HASHES['modules/A2FOShellDisplay/canvas.hpp']:
        raise RuntimeError('frozen Canvas source/header changed')
    if sha(HEADER) != sha(FROZEN_HEADER):
        raise RuntimeError('Canvas shared native header changed')
    libraries = [archive] + ([args.canvas_lib] if args.canvas_lib else [])
    if any(not path.is_file() for path in libraries):
        raise RuntimeError('Canvas Rust archive missing')
    fixtures = {name: ROOT / file for name, (file, _) in TESTS.items()}
    inputs = (FROZEN, SOURCE, HEADER, ASM, *fixtures.values(), *libraries)
    input_hashes = {path: sha(path) for path in inputs}
    def check_inputs():
        for path, initial in input_hashes.items():
            if sha(path) != initial:
                raise RuntimeError(f'input changed during Canvas comparison: {path}')
    WORK.mkdir(parents=True, exist_ok=True)
    executables = {(label, name): compile_fixture(source, label, name,
                   fixture, archive, args.canvas_lib)
                   for label, source in (('frozen', FROZEN), ('current', SOURCE))
                   for name, fixture in fixtures.items()}
    check_inputs()
    if args.compile_only:
        print('Canvas: four frozen/current GNU++17/O2/Werror native fixtures compile PASS; Wine skipped')
        return
    prefix = args.wine_prefix or Path(os.environ.get('A2FO_RUST_WINEPREFIX',
        WORK / 'wine-prefix'))
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        raise RuntimeError('initialized absolute disposable Wine prefix required')
    env = os.environ.copy()
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', DISPLAY='',
               WAYLAND_DISPLAY='',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    transcripts = {}
    for label in ('frozen', 'current'):
        outputs = []
        for name, (_, expected) in TESTS.items():
            output = run(['wine', str(executables[label, name])], env=env,
                         timeout=120)
            check_inputs()
            if not re.fullmatch(expected, output):
                raise RuntimeError(f'unexpected {label}/{name} transcript: {output!r}')
            outputs.append(output)
        transcripts[label] = b''.join(outputs)
    if transcripts['frozen'] != transcripts['current']:
        raise RuntimeError('frozen/current Canvas transcripts differ')
    check_inputs()
    receipt = {'frozen_source_sha256': sha(FROZEN),
               'current_source_sha256': sha(SOURCE),
               'rust_archive_sha256': sha(archive),
               'canvas_lib_sha256': sha(args.canvas_lib) if args.canvas_lib else None,
               'fixture_sha256': {name: sha(file) for name, file in fixtures.items()},
               'compiler': 'GNU++17/O2/Werror, assertions enabled',
               'cases': len(TESTS),
               'transcript_sha256': hashlib.sha256(transcripts['frozen']).hexdigest()}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Canvas: four frozen/current headless native workflows PASS')

if __name__ == '__main__':
    main()
