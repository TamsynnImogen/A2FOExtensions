#!/usr/bin/env python3
"""Compare actual frozen/current A1 physics with synthetic native engine bridges."""
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
WORK = STAGE / 'a1-physics-work'
SOURCE = 'modules/A1Compat/module.cpp'
FIXTURE = ROOT / 'tests/rust_stage8_a1_physics_native.cpp'

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

def closing(text,start,opening,ending):
    depth=0; mode='normal'; index=start
    while index<len(text):
        ch=text[index]; nxt=text[index+1:index+2]
        if mode=='line':
            if ch=='\n':mode='normal'
        elif mode=='block':
            if ch=='*' and nxt=='/':mode='normal';index+=1
        elif mode in ('single','double'):
            if ch=='\\':index+=1
            elif (mode=='single' and ch=="'") or (mode=='double' and ch=='"'):mode='normal'
        elif ch=='/' and nxt=='/':mode='line';index+=1
        elif ch=='/' and nxt=='*':mode='block';index+=1
        elif ch=="'":mode='single'
        elif ch=='"':mode='double'
        elif ch==opening:depth+=1
        elif ch==ending:
            depth-=1
            if depth==0:return index
        index+=1
    raise RuntimeError('unbalanced native function')
def replace_function(text,name,body):
    match=re.search(r'(?m)^bool\s+'+re.escape(name)+r'\s*\(',text)
    if not match:raise RuntimeError('missing '+name)
    opened=text.index('(',match.start());ended=closing(text,opened,'(',')')
    brace=text.find('{',ended+1);semi=text.find(';',ended+1)
    if brace<0 or (0<=semi<brace):raise RuntimeError('not definition '+name)
    close=closing(text,brace,'{','}')
    return text[:brace]+'{'+body+'}'+text[close+1:]
def fixture_source(source):
    text=source.read_text().replace('__declspec(dllexport)','')
    text=re.sub(r'#include "([^"\n]+)"',
        lambda match:'#include "'+str((source.parent/match[1]).resolve())+'"',text)
    return replace_function(text,'parameter_odf_name',
        'return a2fo_fixture_parameter_odf_name(parameter_db,output,output_size);')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--physics-lib', type=Path, help='isolated new-symbol Rust rlib')
    args = parser.parse_args(); WORK.mkdir(parents=True, exist_ok=True)
    archive = Path(os.environ.get('A2FO_RUST_WIN_LIB', ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve()
    before = STAGE / 'before' / SOURCE
    if sha(before) != json.loads((STAGE / 'before-hashes.json').read_text())[SOURCE]:
        raise RuntimeError('frozen A1 source changed')
    pinned = [ROOT / SOURCE, ROOT / 'rust/a2fo_core/src/stage8_a1_physics.rs',
              ROOT / 'rust/stage8_a1_physics_ffi.h', FIXTURE, Path(__file__).resolve(), archive]
    if args.physics_lib: pinned.append(args.physics_lib.resolve())
    initial = {str(path): sha(path) for path in pinned}
    executables = []
    for label, source in [('before', before), ('current', ROOT / SOURCE)]:
        generated = WORK / (label + '-physics.cpp'); generated.write_text(fixture_source(source))
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
            if name.startswith(('_a2fo_a1_', '__ZN8a1compat', '__ZNK8a1compat')): native.append(name)
        stubs = WORK / (label + '-uninvoked.S')
        stubs.write_text('.text\n' + ''.join('.globl ' + name + '\n' + name + ':\n jmp _a2fo_a1_fixture_unexpected_native\n' for name in native))
        trap = WORK / 'uninvoked.cpp'
        trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_a1_fixture_unexpected_native(){std::fputs("unexpected native A1 engine path\\n",stderr);std::abort();}\n')
        libs = [str(args.physics_lib.resolve())] if args.physics_lib else []
        libs.append(str(archive))
        run(['i686-w64-mingw32-g++', '-static', '-Wl,--gc-sections', str(obj), str(stubs), str(trap),
             *libs, '-ladvapi32', '-lgdi32', '-lcomctl32', '-lversion', '-lws2_32', '-lshell32',
             '-lwinmm', '-lole32', '-lmsimg32', '-lgdiplus', '-ldxguid', '-o', str(exe)])
        executables.append(exe)
    if any(sha(path) != initial[str(path)] for path in pinned): raise RuntimeError('A1 physics input drift')
    if args.build_only:
        print('A1 physics frozen/current actual-source GNU++17/O2/Werror fixtures build: PASS')
        return
    prefix = Path(os.environ.get('A2FO_RUST_WINEPREFIX', '/tmp/a2fo-shields-wine-prefix'))
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        raise RuntimeError('initialized absolute private Wine prefix required')
    env = os.environ.copy(); env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', DISPLAY='',
        WAYLAND_DISPLAY='', WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    outputs=[]
    for label,exe in zip(('before','current'),executables):
        output=run(['wine',str(exe)],env=env,timeout=180)
        required=[b'physics:none:0',b'physics:impulse:1',b'physics:warp:1',
                  b'profile:construction:',b'profile:battle:',b'profile:destroyer:',
                  b'A1 physics native fixture: PASS']
        for marker in required:
            if marker not in output:raise RuntimeError(f'{label} missed {marker!r}')
        (WORK/(label+'.txt')).write_bytes(output);outputs.append(output)
    if outputs[0]!=outputs[1]:
        old,new=(value.splitlines() for value in outputs)
        for index in range(max(len(old),len(new))):
            if old[index:index+1]!=new[index:index+1]:
                raise RuntimeError(f'first difference line {index+1}: {old[index:index+1]} != {new[index:index+1]}')
        raise RuntimeError('A1 physics transcript mismatch')
    if any(sha(path) != initial[str(path)] for path in pinned): raise RuntimeError('A1 physics input drift')
    receipt = {'scope': 'three A1 physics functions; mocked ParameterDB name helper and synthetic getter RVA thunk',
               'inputs': initial, 'lines': len(outputs[0].splitlines()),
               'sha256': hashlib.sha256(outputs[0]).hexdigest(), 'compiler': 'GNU++17/O2/Werror'}
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('A1 physics frozen/current native traces:', receipt['lines'], 'PASS')
if __name__ == '__main__': main()
