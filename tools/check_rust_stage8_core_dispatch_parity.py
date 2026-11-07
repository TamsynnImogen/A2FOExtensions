#!/usr/bin/env python3
"""Compare actual frozen/current core dispatch workflows with mocked native bridges."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build/rust-migration-stage8'
WORK=STAGE/'core-dispatch-work'
SOURCE='core/dllmain.cpp'
FIXTURE=ROOT/'tests/rust_stage8_core_dispatch_native.cpp'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(command,env=None,timeout=300):
    with tempfile.TemporaryFile() as out,tempfile.TemporaryFile() as err:
        try:result=subprocess.run(command,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            out.seek(0);err.seek(0)
            raise RuntimeError(f'timed out: {command}\n{out.read()[-2000:]!r}\n{err.read()[-4000:]!r}') from exc
        out.seek(0);err.seek(0);output,error=out.read(),err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{output[-2000:]!r}\n{error[-5000:]!r}')
    return output.replace(b'\r\n',b'\n')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only',action='store_true')
    parser.add_argument('--dispatch-lib',type=Path,help='isolated new-symbol Rust rlib before canonical registration')
    args=parser.parse_args();WORK.mkdir(parents=True,exist_ok=True)
    archive=Path(os.environ.get('A2FO_RUST_WIN_LIB',ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve()
    before=STAGE/'before'/SOURCE
    hashes=json.loads((STAGE/'before-hashes.json').read_text())
    if sha(before)!=hashes[SOURCE]:raise RuntimeError('frozen core source changed')
    pinned=[ROOT/SOURCE,ROOT/'rust/a2fo_core/src/stage8_core_dispatch.rs',ROOT/'rust/stage8_core_dispatch_ffi.h',FIXTURE,Path(__file__).resolve(),archive]
    if args.dispatch_lib:pinned.append(args.dispatch_lib.resolve())
    initial={str(path):sha(path) for path in pinned}
    executables=[]
    for label,source in [('before',before),('current',ROOT/SOURCE)]:
        content=source.read_text().replace('__declspec(dllexport)','')
        content=re.sub(r'\bDllMain\b','a2fo_uninvoked_fixture_DllMain',content)
        content=re.sub(r'\bA2FO_Initialize\b','a2fo_uninvoked_fixture_Initialize',content)
        content=re.sub(r'#include "([^"\n]+)"',
            lambda match:'#include "'+str((source.parent/match[1]).resolve())+'"',content)
        generated=WORK/(label+'-core.cpp');generated.write_text(content)
        obj=WORK/(label+'.o');exe=WORK/(label+'.exe')
        flags=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror',
               '-DNDEBUG','-DWIN32_LEAN_AND_MEAN','-ffunction-sections','-fdata-sections','-static']
        run(flags+[f'-DCORE_SOURCE="{generated}"',str(FIXTURE),'-c','-o',str(obj)])
        undefined=subprocess.check_output(['i686-w64-mingw32-nm','--undefined-only',str(obj)],text=True)
        native=[]
        for line in undefined.splitlines():
            name=line.split()[-1]
            if (name.startswith(('__ZN4a2fo','__ZNK4a2fo','_a2fo_'))
                and not name.startswith('_a2fo_rs_')):native.append(name)
        stubs=WORK/(label+'-uninvoked.S')
        stubs.write_text('.text\n'+''.join('.globl '+name+'\n'+name+':\n jmp _a2fo_fixture_unexpected_native\n' for name in native))
        trap=WORK/'uninvoked.cpp'
        trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_fixture_unexpected_native(){std::fputs("unexpected native engine path\\n",stderr);std::abort();}\n')
        libraries=[str(args.dispatch_lib.resolve())] if args.dispatch_lib else []
        libraries.append(str(archive))
        run(['i686-w64-mingw32-g++','-static','-Wl,--gc-sections',str(obj),str(stubs),str(trap),*libraries,
             '-ladvapi32','-lgdi32','-lcomctl32','-lversion','-lws2_32','-lshell32','-lwinmm',
             '-lole32','-lmsimg32','-lgdiplus','-ldxguid','-o',str(exe)])
        executables.append(exe)
    if any(sha(path)!=initial[str(path)] for path in pinned):raise RuntimeError('core dispatch input changed during build')
    if args.build_only:
        print('core dispatch frozen/current GNU++17/O2/Werror whole-source fixtures build: PASS')
        return
    prefix=Path(os.environ.get('A2FO_RUST_WINEPREFIX','/tmp/a2fo-shields-wine-prefix'))
    if not prefix.is_absolute() or not (prefix/'system.reg').is_file():
        raise RuntimeError('initialized absolute private Wine prefix required')
    env=os.environ.copy();env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',DISPLAY='',WAYLAND_DISPLAY='',
        WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    outputs=[]
    for label,exe in zip(('before','current'),executables):
        output=run(['wine',str(exe)],env=env,timeout=180)
        if (output.count(b'trigger-result:')!=10 or output.count(b'craft-result:')!=3
            or b'class-normal:AC:' not in output or b'class-flip:A:' not in output
            or b'class-throw:AC:' not in output or b'core dispatch workflow: PASS\n' not in output):
            raise RuntimeError(f'{label} fixture missed an expected dispatch scenario')
        (WORK/(label+'.txt')).write_bytes(output);outputs.append(output)
    if outputs[0]!=outputs[1]:
        old,new=(output.splitlines() for output in outputs)
        for index in range(max(len(old),len(new))):
            if old[index:index+1]!=new[index:index+1]:
                raise RuntimeError(f'first difference line {index+1}: {old[index:index+1]} != {new[index:index+1]}')
        raise RuntimeError('core dispatch transcript mismatch')
    if any(sha(path)!=initial[str(path)] for path in pinned):raise RuntimeError('core dispatch input changed during run')
    receipt={'scope':'three core dispatch functions; native ParameterDB bridges mocked',
        'inputs':initial,'lines':len(outputs[0].splitlines()),
        'sha256':hashlib.sha256(outputs[0]).hexdigest(),'compiler':'GNU++17/O2/Werror'}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('core dispatch frozen/current native traces:',receipt['lines'],'PASS')
if __name__=='__main__':main()
