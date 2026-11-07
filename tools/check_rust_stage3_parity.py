#!/usr/bin/env python3
"""Compare the fixed 29-unit Rust milestone against its frozen C++ sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def run(command,env=None):
    with tempfile.TemporaryFile() as stdout,tempfile.TemporaryFile() as stderr:
        p=subprocess.run(command,cwd=ROOT,env=env,stdout=stdout,stderr=stderr,timeout=180)
        stdout.seek(0);stderr.seek(0);out=stdout.read();err=stderr.read()
    if p.returncode:
        raise RuntimeError(f"exit {p.returncode}: {command}\n{out[-1500:]!r}\n{err[-3500:]!r}")
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--group',choices=['all','config','gameplay','ui','lead'],default='all')
    p.add_argument('--windows',action='store_true')
    p.add_argument('--baseline',type=Path,default=ROOT/'build/rust-migration-stage3/before')
    p.add_argument('--wine-prefix',type=Path,default=Path('/tmp/a2fo-engine-compat-wine'))
    p.add_argument('--receipt',type=Path)
    args=p.parse_args();scope=json.loads((ROOT/'build/rust-migration-stage3/scope.json').read_text())
    target='i686-pc-windows-gnu/release' if args.windows else 'release'
    archive=ROOT/'build/rust'/target/'liba2fo_core.a'
    inputs=[*(ROOT/'rust/a2fo_core/src').glob('*.rs'),ROOT/'rust/a2fo_core/Cargo.toml',ROOT/'rust/a2fo_core/Cargo.lock']
    if not archive.is_file() or any(x.stat().st_mtime_ns>archive.stat().st_mtime_ns for x in inputs):
        p.error(f'{target} archive is missing or stale; build it after source changes')
    env=os.environ.copy();runner=[]
    if args.windows:
        for key in ['DISPLAY','WAYLAND_DISPLAY']:env.pop(key,None)
        env.update(WINEPREFIX=str(args.wine_prefix.resolve()),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
        runner=['wine']
    archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest()
    groups=list(scope['groups']) if args.group=='all' else [args.group];results=[]
    for group in groups:
        units=list(scope['groups'][group])
        if group=='lead' and not args.windows:units.remove('renderer/state/fvf.cpp')
        directories=list(dict.fromkeys(str(Path(x).parent) for x in units))
        fixture=ROOT/'tests'/f'rust_{group}_parity.cpp'
        with tempfile.TemporaryDirectory(prefix='a2fo-rust25-') as temp:
            outputs=[]
            for old in (True,False):
                base=args.baseline.resolve() if old else ROOT
                exe=Path(temp)/(('before' if old else 'rust')+('.exe' if args.windows else ''))
                cmd=['i686-w64-mingw32-g++' if args.windows else 'g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-pthread']
                if args.windows:cmd+=['-static']
                for directory in directories:cmd+=['-I',str(base/directory)]
                # Transitive SDK/presentation headers remain unchanged. Each
                # baseline unit and its own public header still take priority.
                if old:
                    for directory in directories:cmd+=['-I',str(ROOT/directory)]
                cmd += [str(fixture),*(str(base/unit) for unit in units)]
                if not old:cmd+=[str(archive)]
                cmd+=['-o',str(exe)];run(cmd);outputs.append(run(runner+[str(exe)],env))
            before,after=outputs
            if before!=after:
                a,b=before.splitlines(),after.splitlines()
                for i in range(max(len(a),len(b))):
                    x=a[i] if i<len(a) else b'<missing>';y=b[i] if i<len(b) else b'<missing>'
                    if x!=y:
                        print(f'{group} differs at line {i+1}\nbefore: {x[:1200]!r}\nrust:   {y[:1200]!r}',file=sys.stderr);return 1
            item={'group':group,'cpp_units':units,'cases':len(after.splitlines()),'sha256':hashlib.sha256(after).hexdigest(),'equal':True,'fixture_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),
                  'before_cpp_sha256':{u:hashlib.sha256((args.baseline/u).read_bytes()).hexdigest() for u in units},
                  'current_cpp_sha256':{u:hashlib.sha256((ROOT/u).read_bytes()).hexdigest() for u in units}}
            results.append(item);print(f"{group}: {item['cases']} traces, {item['sha256']}, PASS",flush=True)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=archive_sha256:
        raise RuntimeError('Rust archive changed during comparison; rerun after build finishes')
    if args.receipt:
        args.receipt.parent.mkdir(parents=True,exist_ok=True)
        args.receipt.write_text(json.dumps({'windows':args.windows,'archive_sha256':archive_sha256,'results':results},indent=2)+'\n')
    return 0

if __name__=='__main__':
    try:sys.exit(main())
    except Exception as e:print(e,file=sys.stderr);sys.exit(2)
