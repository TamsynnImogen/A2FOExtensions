#!/usr/bin/env python3
"""Actual-source A1 text policy comparison; native engine paths stay trapped."""
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
WORK=STAGE/'a1-work'
SOURCE='modules/A1Compat/module.cpp'
FIXTURE=ROOT/'tests/rust_stage8_a1_native.cpp'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(command,env=None,allowed=(0,),timeout=300):
    with tempfile.TemporaryFile()as out,tempfile.TemporaryFile()as err:
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=timeout)
        out.seek(0);err.seek(0);output,error=out.read(),err.read()
    if result.returncode not in allowed:
        raise RuntimeError(f'{command}: {result.returncode}\n{output[-3000:]!r}\n{error[-5000:]!r}')
    return output.replace(b'\r\n',b'\n')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only',action='store_true')
    parser.add_argument('--a1-lib',type=Path,help='isolated A1 Rust rlib before canonical registration')
    args=parser.parse_args();WORK.mkdir(parents=True,exist_ok=True)
    archive=Path(os.environ.get('A2FO_RUST_WIN_LIB',ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve()
    frozen=STAGE/'before'/SOURCE
    assert sha(frozen)==json.loads((STAGE/'before-hashes.json').read_text())[SOURCE]
    pinned=[ROOT/SOURCE,ROOT/'rust/a2fo_core/src/stage8_a1.rs',ROOT/'rust/stage8_a1_ffi.h',FIXTURE,Path(__file__).resolve(),archive]
    if args.a1_lib:pinned.append(args.a1_lib.resolve())
    initial={str(p):sha(p)for p in pinned}
    executables=[]
    for label,source in [('before',frozen),('current',ROOT/SOURCE)]:
        text=source.read_text().replace('__declspec(dllexport)','')
        text=re.sub(r'#include "([^"\n]+)"',lambda m:'#include "'+str((source.parent/m[1]).resolve())+'"',text)
        generated=WORK/(label+'-a1.cpp');generated.write_text(text)
        obj=WORK/(label+'.o');exe=WORK/(label+'.exe')
        flags=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-error=cast-function-type','-DNDEBUG','-DWIN32_LEAN_AND_MEAN','-ffunction-sections','-fdata-sections','-static']
        run(flags+[f'-DA1_SOURCE="{generated}"',str(FIXTURE),'-c','-o',str(obj)])
        undefined=subprocess.check_output(['i686-w64-mingw32-nm','--undefined-only',str(obj)],text=True)
        native=[]
        for line in undefined.splitlines():
            name=line.split()[-1]
            if name.startswith(('_a2fo_a1_','__ZN8a1compat','__ZNK8a1compat')):native.append(name)
        stubs=WORK/(label+'-uninvoked.S')
        stubs.write_text('.text\n'+''.join('.globl '+n+'\n'+n+':\n jmp _a2fo_a1_fixture_unexpected_native\n'for n in native))
        trap=WORK/'uninvoked.cpp';trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_a1_fixture_unexpected_native(){std::fputs("unexpected native A1 engine path\\n",stderr);std::abort();}\n')
        link=[str(obj),str(stubs),str(trap)]
        if args.a1_lib:link.append(str(args.a1_lib.resolve()))
        link.append(str(archive))
        run(flags+['-Wl,--gc-sections']+link+['-ladvapi32','-lgdi32','-lcomctl32','-lversion','-lws2_32','-lshell32','-lwinmm','-lole32','-lmsimg32','-lgdiplus','-ldxguid','-o',str(exe)])
        executables.append(exe)
    if args.build_only:
        assert all(sha(p)==initial[str(p)]for p in pinned),'A1 build input changed during run'
        print('A1 actual-source frozen/current text fixtures build: PASS');return
    env=os.environ.copy();env.update(WINEPREFIX=os.environ.get('A2FO_RUST_WINEPREFIX','/tmp/a2fo-shields-wine-prefix'),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d');env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None)
    transcripts={}
    for mode in ('text','bool-terminate'):
        outputs=[]
        for label,exe in zip(('before','current'),executables):
            output=run(['wine',str(exe)]+([]if mode=='text'else[mode]),env,allowed=(0,)if mode=='text'else(77,),timeout=240)
            (WORK/(label+'-'+mode+'.txt')).write_bytes(output);outputs.append(output)
        if outputs[0]!=outputs[1]:
            old,new=(x.splitlines()for x in outputs)
            for index in range(max(len(old),len(new))):
                if old[index:index+1]!=new[index:index+1]:raise RuntimeError(f'{mode} first difference line{index+1}: {old[index:index+1]} != {new[index:index+1]}')
            raise RuntimeError('trace mismatch')
        transcripts[mode]={'lines':len(outputs[0].splitlines()),'sha256':hashlib.sha256(outputs[0]).hexdigest()}
    assert all(sha(p)==initial[str(p)]for p in pinned),'A1 comparison input changed during run'
    receipt={'scope':'A1 text helper policies only; unit remains partial; native engine entrypoints uncalled','inputs':initial,'transcripts':transcripts,'compiler':'GNU++17/O2; preexisting FARPROC cast warning allowed equally in frozen/current'}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('A1 text policies:',transcripts,'PASS')
if __name__=='__main__':main()
