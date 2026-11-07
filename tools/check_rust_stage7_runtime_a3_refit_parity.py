#!/usr/bin/env python3
"""Run frozen/current Camera and View DLLs through the native PE fixture."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parent.parent
WORK=ROOT/'build/rust-migration-stage7/runtime-a-work'
BEFORE=ROOT/'build/rust-migration-stage7/before'
RUST=Path(os.environ.get('A2FO_RUST_WIN_LIB',WORK/'liba3_windows.a'))
CASES=('missing','disabled','invalid','noop','api','foreign','context','identity',
       'relocation','patch-fail','success')
def run(args,**kwargs):
    return subprocess.run(args,cwd=ROOT,check=True,**kwargs)
def build(name,version):
    source=(BEFORE if version=='before' else ROOT)/'modules'/name/'module.cpp'
    bridge=ROOT/'modules'/name/'native_bridge.S'
    out=WORK/f'{name}-{version}.dll'
    command=['i686-w64-mingw32-g++','-DWIN32_LEAN_AND_MEAN','-std=gnu++17','-O2',
        '-Wall','-Wextra','-Wpedantic','-DNDEBUG','-Werror','-shared','-static',
        '-static-libgcc','-static-libstdc++','-Wl,--enable-stdcall-fixup',
        '-o',str(out),str(source),str(bridge)]
    if version=='current':command.append(str(RUST))
    command+=['-ladvapi32','-lgdi32']
    run(command)
    return out
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
    env=os.environ.copy();env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None);env['WINEPREFIX']=str(os.environ.get('A2FO_RUST_WINEPREFIX', WORK/'wine-prefix'));env['WINEDEBUG']='-all'
    env['WINEDLLOVERRIDES']='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
    count=0
    for name in ('A2FOCameraControls','A2FOViewDistance'):
        dlls={version:build(name,version) for version in ('before','current')}
        cases=CASES+(('consumer',) if 'Camera' in name else ())
        for case in cases:
            for version,dll in dlls.items():
                result=wine_capture(['wine',str(ROOT/'build/refit_module_smoke.exe'),str(dll),
                            str(ROOT/'ArmadaL.exe'),case],env=env,text=True,timeout=30)
                if 'PASS' not in result.stdout:
                    raise RuntimeError(f'{name} {version} {case}: {result.stdout} {result.stderr}')
                count+=1
            print(f'{name} {case}: frozen/current native fixture PASS',flush=True)
    print(f'{count}/{count} native fixture processes passed')
    return 0
if __name__=='__main__':sys.exit(main())
