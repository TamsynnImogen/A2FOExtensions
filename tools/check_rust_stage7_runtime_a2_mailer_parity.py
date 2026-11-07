#!/usr/bin/env python3
"""Frozen/current LogMailer module differential under 32-bit Wine."""
from pathlib import Path
import difflib,os,subprocess,sys
ROOT=Path(__file__).resolve().parent.parent
WORK=ROOT/'build/rust-migration-stage7/runtime-a-work'
BEFORE=ROOT/'build/rust-migration-stage7/before'
TEST=ROOT/'tests/rust_stage7_runtime_a2_mailer_parity.cpp'
LIB=Path(os.environ.get('A2FO_RUST_WIN_LIB',WORK/'cargo/i686-pc-windows-gnu/release/liba2fo_core.a'))
def run(args,**kwargs):
    print('+',' '.join(map(str,args)),flush=True)
    return subprocess.run(args,cwd=ROOT,check=True,**kwargs)
def wine_capture(args, env, timeout=120, text=False, check=True):
    import tempfile
    print('+', ' '.join(map(str, args)), flush=True)
    mode = 'w+t' if text else 'w+b'
    with tempfile.TemporaryFile(mode=mode) as stdout, tempfile.TemporaryFile(mode=mode) as stderr:
        result = subprocess.run(args, cwd=ROOT, env=env, stdout=stdout, stderr=stderr,
                                timeout=timeout)
        stdout.seek(0); stderr.seek(0)
        output, errors = stdout.read(), stderr.read()
    if check and result.returncode:
        raise RuntimeError(f'Wine fixture exited {result.returncode}: {args}\n{output}\n{errors}')
    return subprocess.CompletedProcess(args, result.returncode, output, errors)

def main():
    transcripts=[]
    for version in ('before','current'):
        source=(BEFORE if version=='before' else ROOT)/'modules/A2FOLogMailer/module.cpp'
        exe=WORK/f'mailer-{version}.exe'
        args=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-DNDEBUG','-static',f'-DMODULE_SOURCE="{source}"',str(TEST)]
        if version=='current':args+=[str(LIB)]
        args+=['-lws2_32','-lbcrypt','-lole32','-ladvapi32','-o',str(exe)]
        run(args)
        env=os.environ.copy();env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None);env['WINEPREFIX']=str(os.environ.get('A2FO_RUST_WINEPREFIX', WORK/'wine-prefix'));env['WINEDEBUG']='-all'
        env['WINEDLLOVERRIDES']='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
        output=wine_capture(['wine',str(exe)],env=env,text=True,timeout=120).stdout
        (WORK/f'mailer-{version}.txt').write_text(output);transcripts.append(output)
    if transcripts[0]!=transcripts[1]:
        print(''.join(difflib.unified_diff(transcripts[0].splitlines(True),
            transcripts[1].splitlines(True),fromfile='frozen',tofile='current')))
        return 1
    print(f'LogMailer: {len(transcripts[0].splitlines())} matching trace lines')
    return 0
if __name__=='__main__':sys.exit(main())
