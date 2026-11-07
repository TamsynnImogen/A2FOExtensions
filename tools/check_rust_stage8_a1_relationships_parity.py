#!/usr/bin/env python3
"""Compare actual frozen/current A1 relationships with synthetic native engine bridges."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / 'build/rust-migration-stage8'
WORK = STAGE / 'a1-relationships-work'
SOURCE = 'modules/A1Compat/module.cpp'
FIXTURE = ROOT / 'tests/rust_stage8_a1_relationships_native.cpp'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, env=None, timeout=300, expected=0):
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=out, stderr=err, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            out.seek(0); err.seek(0)
            raise RuntimeError(f'timeout {command}: {out.read()[-2000:]!r} {err.read()[-4000:]!r}') from exc
        out.seek(0); err.seek(0)
        output, error = out.read(), err.read()
    if result.returncode != expected:
        raise RuntimeError(f'{command}: exit {result.returncode} expected {expected}: {output[-2000:]!r} {error[-5000:]!r}')
    return output.replace(b'\r\n', b'\n')

def fixture_source(source):
    text = source.read_text().replace('__declspec(dllexport)', '')
    return re.sub(r'#include "([^"\n]+)"',
                  lambda match: '#include "' + str((source.parent / match[1]).resolve()) + '"', text)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--relationships-lib', type=Path, help='isolated new-symbol Rust rlib')
    args = parser.parse_args(); WORK.mkdir(parents=True, exist_ok=True)
    archive = Path(os.environ.get('A2FO_RUST_WIN_LIB', ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve()
    before = STAGE / 'before' / SOURCE
    if sha(before) != json.loads((STAGE / 'before-hashes.json').read_text())[SOURCE]:
        raise RuntimeError('frozen A1 source changed')
    pinned = [ROOT / SOURCE, ROOT / 'rust/a2fo_core/src/stage8_a1_relationships.rs',
              ROOT / 'rust/stage8_a1_relationships_ffi.h', FIXTURE, Path(__file__).resolve(), archive]
    if args.relationships_lib: pinned.append(args.relationships_lib.resolve())
    initial = {str(path): sha(path) for path in pinned}
    executables = []
    for label, source in [('before', before), ('current', ROOT / SOURCE)]:
        generated = WORK / (label + '-relationships.cpp'); generated.write_text(fixture_source(source))
        obj = WORK / (label + '.o'); exe = WORK / (label + '.exe')
        flags = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                 '-Werror', '-Wno-error=cast-function-type', '-Wno-unused-function',
                 '-Wno-unused-variable', '-Wno-unused-parameter', '-DNDEBUG',
                 '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections', '-fdata-sections', '-static']
        run(flags + [f'-DA1_SOURCE="{generated}"', str(FIXTURE), '-c', '-o', str(obj)])
        undefined = subprocess.check_output(['i686-w64-mingw32-nm', '--undefined-only', str(obj)], text=True)
        native = []
        for line in undefined.splitlines():
            name = line.split()[-1]
            if name.startswith(('_a2fo_a1_', '__ZN8a1compat', '__ZNK8a1compat')) and name not in ('_a2fo_a1_call_thiscall_0', '_a2fo_a1_call_thiscall_1', '_a2fo_a1_call_thiscall_2'): native.append(name)
        stubs = WORK / (label + '-uninvoked.S')
        stubs.write_text('.text\n' + ''.join('.globl ' + name + '\n' + name + ':\n jmp _a2fo_a1_fixture_unexpected_native\n' for name in native))
        trap = WORK / 'uninvoked.cpp'
        trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_a1_fixture_unexpected_native(){std::fputs("unexpected native A1 engine path\\n",stderr);std::abort();}\n')
        libs = [str(args.relationships_lib.resolve())] if args.relationships_lib else []
        libs.append(str(archive))
        run(['i686-w64-mingw32-g++', '-static', '-Wl,--gc-sections', str(obj), str(stubs), str(trap),
             *libs, '-ladvapi32', '-lgdi32', '-lcomctl32', '-lversion', '-lws2_32', '-lshell32',
             '-lwinmm', '-lole32', '-lmsimg32', '-lgdiplus', '-ldxguid', '-o', str(exe)])
        executables.append(exe)
    if any(sha(path) != initial[str(path)] for path in pinned): raise RuntimeError('A1 relationships input drift')
    if args.build_only:
        print('A1 relationships frozen/current actual-source GNU++17/O2/Werror fixtures build: PASS')
        return
    prefix = Path(os.environ.get('A2FO_RUST_WINEPREFIX', '/tmp/a2fo-shields-wine-prefix'))
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        raise RuntimeError('initialized absolute private Wine prefix required')
    env = os.environ.copy(); env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', DISPLAY='',
        WAYLAND_DISPLAY='', WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    counts = {}
    for mode, expected_exit in [('normal', 0)]:
        outputs = []
        for label, exe in zip(('before', 'current'), executables):
            command = ['wine', str(exe)]
            output = run(command, env=env, timeout=180, expected=expected_exit)
            (WORK / (label + '-' + mode + '.txt')).write_bytes(output); outputs.append(output)
        required = [b'case:allied\n', b'active:0:1\n', b'lookup:0\n',
                    b'set:1:2:2\n', b'case:enemy\n', b'set:1:2:0\n',
                    b'case:no-active\n',
                    b'log:A1 Instant Action relationship restore found no active setup slots\n',
                    b'case:mutation\n', b'mutate:slot1-team=7\n',
                    b'case:throw-set\n',
                    b'log:A1 Instant Action relationship restore failed safely\n',
                    b'case:before-active\n', b'get-setup:1\n',
                    b'A1 relationships native fixture: PASS\n']
        for required_line in required:
            if required_line not in outputs[0]:
                raise RuntimeError(f'frozen relationship scenario was not exercised: {required_line!r}')
        if outputs[0] != outputs[1]:
            old, new = (output.splitlines() for output in outputs)
            for index in range(max(len(old), len(new))):
                if old[index:index+1] != new[index:index+1]:
                    raise RuntimeError(f'{mode} first difference line {index+1}: {old[index:index+1]} != {new[index:index+1]}')
            raise RuntimeError(f'{mode} A1 relationships transcript mismatch')
        counts[mode] = {'lines': len(outputs[0].splitlines()), 'sha256': hashlib.sha256(outputs[0]).hexdigest(),
                        'exit': expected_exit}
    if any(sha(path) != initial[str(path)] for path in pinned): raise RuntimeError('A1 relationships input drift')
    receipt = {'scope': 'A1_ModuleRelationships only; synthetic executable RVA thunks and native SDK bridge mocks in both actual sources',
               'inputs': initial, 'modes': counts, 'compiler': 'GNU++17/O2/Werror'}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('A1 relationships frozen/current native traces:', counts, 'PASS')
if __name__ == '__main__': main()
